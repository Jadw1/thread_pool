#include "future.h"
#include <stdio.h>
#include "err.h"

#define UNUSED __attribute__((unused))

typedef struct factorial {
  int n;
  unsigned long long result;
} factorial_t;

void* count_factorial(void* arg, UNUSED size_t args, UNUSED size_t* rezs) {
  factorial_t* fac = (factorial_t*)arg;
  fac->result = (fac->n == 0) ? 1 : fac->result * fac->n;
  fac->n++;
  
  *rezs = 1;
  return fac;
}

int main() {
  int n, err;
  scanf("%d", &n);
  
  thread_pool_t pool;
  if((err = thread_pool_init(&pool, 3)) != 0)
    syserr(err, "Thread pool init.");

  factorial_t* fac = malloc(sizeof(factorial_t));
  if(fac == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    exit(-1);
  }
  fac->n = 0;
  fac->result = 1;

  future_t* futures = malloc(sizeof(future_t) * (n + 1));
  if(futures == NULL) {
    fprintf(stderr, "Unable to allocate memory.");
    exit(-1);
  }
  callable_t callable;
  callable.function = &count_factorial;
  callable.arg = fac;
  callable.argsz = 1;
  
  if((err = async(&pool, futures, callable)) != 0)
    syserr(err, "Async failed.");
  for(int i = 1; i <= n; i++) {
    if((err = map(&pool, futures + i, futures + (i - 1), &count_factorial)) != 0)
      syserr(err, "Map failed.");
  }
  factorial_t* result = (factorial_t*)await(futures + n);
  printf("%llu\n", result->result);

  for(int i = 0; i <= n; i++) {
    future_destroy(futures + i);
  }

  thread_pool_destroy(&pool);

  free(result);
  free(futures);

  return 0;
}