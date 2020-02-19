#include "threadpool.h"
#include <signal.h>
#include <unistd.h>
#include "err.h"

typedef struct pool_list pool_list_t;
struct pool_list {
  thread_pool_t* pool;

  pool_list_t* prev;
  pool_list_t* next;
};

pthread_mutex_t list_mutex;
pool_list_t* pool_list;
bool block_pool;

void handler(__attribute__((unused))int id) {
  int err;
  if((err = pthread_mutex_lock(&list_mutex)) != 0) {
    syserr(err, "Mutex lock failed.");
  }

  block_pool = true;
  pool_list_t* node = pool_list;
  while(node) {
    if((err = pthread_mutex_unlock(&list_mutex)) != 0) {
      syserr(err, "Mutex unlock failed.");
    }

    pool_list_t* tmp = node->next;
    thread_pool_destroy(node->pool);
    node = tmp;

    if((err = pthread_mutex_lock(&list_mutex)) != 0) {
      syserr(err, "Mutex lock failed.");
    }
  }

  if((err = pthread_mutex_unlock(&list_mutex)) != 0) {
    syserr(err, "Mutex unlock failed.");
  }
  _exit(0);
}

__attribute__((constructor))
void change_signal() {
  int err;
  if((err = pthread_mutex_init(&list_mutex, NULL)) != 0)
    syserr(err, "Mutex init failed.");
  pool_list = NULL;
  block_pool = false;

  struct sigaction action;
  sigset_t block_mask;

  sigemptyset(&block_mask);
  action.sa_handler = handler;
  action.sa_mask = block_mask;
  action.sa_flags = 0;

  if((err = sigaction(SIGINT, &action, 0)) != 0) {
    syserr(err, "Sigaction failed.");
  }
}

__attribute__((destructor))
void destroy_pool_list() {
  int err;
  if((err = pthread_mutex_destroy(&list_mutex)) != 0)
    syserr(err, "Mutex destroy failed");

  while(pool_list) {
    pool_list_t* tmp = pool_list->next;
    free(pool_list);
    pool_list = tmp;
  }
}


static int job_add(thread_pool_t* pool, runnable_t runnable) {
  job_t* job = (job_t*)malloc(sizeof(job_t));
  if(job == NULL) {
    return -1;
  }

  job->runnable = runnable;
  job->next = NULL;

  if(pool->first == NULL) {
    pool->first = job;
    pool->last = job;
  }
  else {
    pool->last->next = job;
    pool->last = job;
  }

  return 0;
}

static job_t* job_get(thread_pool_t* pool) {
  job_t* job = pool->first;
  if(job == NULL) {
    return NULL;
  }

  if(job->next == NULL) {
    pool->first = NULL;
    pool->last = NULL;
  }
  else {
    pool->first = job->next;
  }

  return job;
}

static void job_destroy(job_t* job) {
  if(job != NULL) {
    free(job);
  }
}

static void* thread_pool_worker(void* thread_pool) {
  thread_pool_t* pool = (thread_pool_t*)thread_pool;
  int err;

  while(true) {
    if((err = pthread_mutex_lock(&pool->mutex)) != 0)
      syserr(err, "Mutex lock failed.");

    if(pool->stop && pool->first == NULL) {
      break;
    }

    job_t* job = NULL;
    while(!(job = job_get(pool)) && !pool->stop) {
      if((err = pthread_cond_wait(&pool->work_cond, &pool->mutex)) != 0) {
        syserr(err, "Cond wait failed.");
      }
    }

    pool->working_count++;
    if((err = pthread_mutex_unlock(&pool->mutex)) != 0)
      syserr(err, "Mutex unlock failed.");

    if(job != NULL) {
      job->runnable.function(job->runnable.arg, job->runnable.argsz);
      job_destroy(job);
    }

    if((err = pthread_mutex_lock(&pool->mutex)) != 0)
      syserr(err, "Mutex lock failed.");
    pool->working_count--;
    if((err = pthread_mutex_unlock(&pool->mutex)) != 0)
      syserr(err, "Mutex unlock failed.");
  }

  pool->threads_count--;
  if(pool->threads_count == 0) {
    if((err = pthread_cond_signal(&pool->stop_cond)) != 0)
      syserr(err, "Cond signal failed.");
  }
  if((err = pthread_mutex_unlock(&pool->mutex)) != 0)
    syserr(err, "Mutex unlock failed");

  return NULL;
}

int thread_pool_init(thread_pool_t* pool, size_t pool_size) {
  pool->working_count = 0;
  pool->threads_count = 0;
  pool->stop = false;
  pool->first = NULL;
  pool->last = NULL;

  int err;

  if((err = pthread_mutex_init(&pool->mutex, NULL)) != 0) {
    return err;
  }
  if((err = pthread_cond_init(&pool->work_cond, NULL)) != 0) {
    return err;
  }
  if((err = pthread_cond_init(&pool->stop_cond, NULL)) != 0) {
    return err;
  }

  pthread_t thread;
  for(size_t i = 0; i < pool_size; i++) {
    if((err = pthread_create(&thread, NULL, thread_pool_worker, pool)) != 0) {
      //end all created threads
      pool->stop = true;
      int e;
      if((e = pthread_cond_broadcast(&pool->work_cond)) != 0)
        syserr(e, "Cond broadcast failed.");
      return err;
    }
    if((err = pthread_detach(thread)) != 0) {
      return err;
    }
    pool->threads_count++;
  }

  //adding thread_pool to list
  pool_list_t* node = malloc(sizeof(pool_list_t));
  if(node == NULL) {
    return -1;
  }
  if((err = pthread_mutex_lock(&list_mutex)) != 0)
    syserr(err, "Mutex lock failed.");
  node->pool = pool;
  node->next = pool_list;
  node->prev = NULL;

  pool_list = node;
  if((err = pthread_mutex_unlock(&list_mutex)) != 0)
    syserr(err, "Mutex unlock failed.");

  return 0;
}

void thread_pool_destroy(thread_pool_t* pool) {
  if(pool == NULL) {
    return;
  }
  int err;

  if((err = pthread_mutex_lock(&pool->mutex)) != 0)
    syserr(err, "Mutex lock failed.");

  pool->stop = true;
  if((err = pthread_cond_broadcast(&pool->work_cond)) != 0)
    syserr(err, "Cond broadcast failed.");
  if((err = pthread_cond_wait(&pool->stop_cond, &pool->mutex)) != 0)
    syserr(err, "Cond wait failed.");
  if((err = pthread_mutex_unlock(&pool->mutex)) != 0)
    syserr(err, "Mutex unlock failed.");

  if((err = pthread_mutex_destroy(&pool->mutex)) != 0)
    syserr(err, "Mutex destroy failed");
  if((err = pthread_cond_destroy(&pool->work_cond)) != 0)
    syserr(err, "Cond destroy failed.");
  if((err = pthread_cond_destroy(&pool->stop_cond)) != 0)
    syserr(err, "Cond destroy failed.");
  
  if((err = pthread_mutex_lock(&list_mutex)) != 0)
    syserr(err, "Mutex lock failed.");
  if(pool_list) {
    if(pool_list->pool == pool) {
      pool_list_t* tmp = pool_list->next;
      free(pool_list);
      pool_list = tmp;
    }
    else {
      pool_list_t* node = pool_list;
      while(node) {
        if(node->pool == pool) {
          node->prev->next = node->next;
          if(node->next) {
            node->next->prev = node->prev;
          }
          free(node);
          break;
        }
        else {
          node = node->next;
        }
      }
    }
  }
  if((err = pthread_mutex_unlock(&list_mutex)) != 0)
    syserr(err, "Mutex unlock failed.");
}

int defer(thread_pool_t* pool, runnable_t runnable) {
  if(pool == NULL) {
    return -1;
  }

  int err;

  if((err = pthread_mutex_lock(&pool->mutex)) != 0)
    return err;
  if(pool->stop || block_pool) {
    return -1;
  }
  if((err = job_add(pool, runnable)) != 0) {
    return err;
  }
  
  if((err = pthread_cond_broadcast(&pool->work_cond)) != 0)
    return err;
  if((err = pthread_mutex_unlock(&pool->mutex)) != 0)
    return err;

  return 0;
}