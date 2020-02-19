#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct runnable {
  void (*function)(void *, size_t);
  void* arg;
  size_t argsz;
} runnable_t;

typedef struct job job_t;

struct job {
  runnable_t runnable;
  job_t* next;
};

typedef struct thread_pool {
  size_t working_count;
  size_t threads_count;
  bool stop;

  job_t* first;
  job_t* last;

  pthread_mutex_t mutex;
  pthread_cond_t work_cond;
  pthread_cond_t stop_cond;
} thread_pool_t;

int thread_pool_init(thread_pool_t* pool, size_t pool_size);

void thread_pool_destroy(thread_pool_t* pool);

int defer(thread_pool_t* pool, runnable_t runnable);

#endif