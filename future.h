#ifndef FUTURE_H
#define FUTURE_H

#include "threadpool.h"
#include <pthread.h>

typedef struct callable {
  void *(*function)(void*, size_t, size_t*);
  void* arg;
  size_t argsz;
} callable_t;

typedef struct future future_t;
struct future {
  callable_t callable;
  future_t* take_from;

  bool completed;
  void* result;
  size_t result_size;

  pthread_mutex_t mutex;
  pthread_cond_t complet_cond;
};

int async(thread_pool_t* pool, future_t* future, callable_t callable);

int map(thread_pool_t* pool, future_t* future, future_t* from,
          void *(*function)(void*, size_t, size_t*));

void* await(future_t* future);

void future_destroy(future_t* future);

#endif