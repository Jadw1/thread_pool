#include "future.h"
#include "err.h"

void future_worker(void* arg, __attribute__((unused))size_t args) {
  future_t* fut = (future_t*)arg;
  int err;

  if((err = pthread_mutex_lock(&fut->mutex)) != 0) {
    syserr(err, "Mutex lock failed.");
  }
  if(fut->take_from != NULL) {
    while(!fut->take_from->completed) {
      if((err = pthread_cond_wait(&fut->take_from->complet_cond, &fut->mutex)) != 0)
        syserr(err, "Cond wait failed.");
    }
    fut->callable.arg = fut->take_from->result;
    fut->callable.argsz = fut->take_from->result_size;
  }
  callable_t* call = &fut->callable;
  if((err = pthread_mutex_unlock(&fut->mutex)) != 0)
    syserr(err, "Mutex unlock failed");
  
  void* result = call->function(call->arg, call->argsz, &fut->result_size);

  if((err = pthread_mutex_lock(&fut->mutex)) != 0) {
    syserr(err, "Mutex lock failed.");
  }
  fut->result = result;
  fut->completed = true;
  if((err = pthread_cond_broadcast(&fut->complet_cond)) != 0)
    syserr(err, "Cond broadcast.");
  if((err = pthread_mutex_unlock(&fut->mutex)) != 0)
    syserr(err, "Mutex unlock failed");
}

int async(thread_pool_t* pool, future_t* future, callable_t callable) {
  int err;
  future->completed = false;
  future->result = NULL;
  future->result_size = 0;

  if((err = pthread_mutex_init(&future->mutex, NULL)) != 0) {
    return err;
  }
  if((err = pthread_cond_init(&future->complet_cond, NULL)) != 0) {
    return err;
  }

  future->callable = callable;
  future->take_from = NULL;

  runnable_t runnable;
  runnable.arg = future;
  runnable.argsz = 1;
  runnable.function = &future_worker;

  if((err = defer(pool, runnable)) != 0) {
    return err;
  }

  return 0;
}

void* await(future_t* future) {
  int err;
  if((err = pthread_mutex_lock(&future->mutex)) != 0)
    syserr(err, "Mutex lock failed.");

  while(!future->completed) {
    if((err = pthread_cond_wait(&future->complet_cond, &future->mutex)) != 0)
      syserr(err, "Cond wait failed.");
  }
  
  if((err = pthread_mutex_unlock(&future->mutex)) != 0)
    syserr(err, "Mutex unlock failed.");
  return future->result;
}

int map(thread_pool_t* pool, future_t* future, future_t* from,
          void *(*function)(void*, size_t, size_t*)) {
  int err;
  future->completed = false;
  future->result = NULL;
  future->result_size = 0;

  if((err = pthread_mutex_init(&future->mutex, NULL)) != 0) {
    return err;
  }
  if((err = pthread_cond_init(&future->complet_cond, NULL)) != 0) {
    return err;
  }

  callable_t callable;
  callable.function = function;

  future->callable = callable;
  future->take_from = from;

  runnable_t runnable;
  runnable.arg = future;
  runnable.argsz = 1;
  runnable.function = &future_worker;

  if((err = defer(pool, runnable)) != 0) {
    return err;
  }

  return 0;
}

void future_destroy(future_t* future) {
  int err;
  if((err = pthread_mutex_lock(&future->mutex)) != 0) {
    syserr(err, "Mutex lock failed.");
  }

  if(!future->completed) {
    if((err = pthread_cond_wait(&future->complet_cond, &future->mutex)) != 0)
      syserr(err, "Cond wait failed.");
  }

  if((err = pthread_mutex_unlock(&future->mutex)) != 0)
    syserr(err, "Mutex unlock failed.");

  if((err = pthread_cond_destroy(&future->complet_cond)) != 0)
    syserr(err, "Cond destroy failed.");
  if((err = pthread_mutex_destroy(&future->mutex)) != 0)
    syserr(err, "Mutex destroy failed.");
}