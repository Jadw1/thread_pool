#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <math.h>
#include "err.h"

typedef struct cell {
  int value;
  __useconds_t time;
  int* row;
  pthread_mutex_t* mutex;
} cell_t;

void calculate_cell(void* arg, __attribute__((unused))size_t args) {
  cell_t* cell = (cell_t*)arg;
  int err;

  usleep(cell->time * 1000);
  if((err = pthread_mutex_lock(cell->mutex)) != 0)
    syserr(err, "Mutex lock failed.");
  *(cell->row) += cell->value;
  if((err = pthread_mutex_unlock(cell->mutex)) != 0)
    syserr(err, "Mutex unlock failed.");
}

int main() {
  int err;
  thread_pool_t pool;
  size_t pool_size = 4;

  if((err = thread_pool_init(&pool, pool_size)) != 0)
    syserr(err, "Thread pool init failed.");

  int rows, columns;
  scanf("%d", &rows);
  scanf("%d", &columns);

  int* matrix = calloc(rows, sizeof(int));
  pthread_mutex_t* row_mutex = malloc(sizeof(pthread_mutex_t) * rows);
  cell_t* cells = malloc(sizeof(cell_t) * rows * columns);
  if(matrix == NULL || row_mutex == NULL || cells == NULL) {
    fprintf(stderr, "Unable to alloc memory.");
    exit(-1);
  }

  for(int i = 0; i < rows; i++) {
    if((err = pthread_mutex_init(row_mutex + i, NULL)) != 0)
      syserr(err, "Mutex init failed.");
  }

  runnable_t runnable;
  runnable.function = &calculate_cell;
  runnable.argsz = 1;
  for(int i = 0; i < rows * columns; i++) {
    int v;
    __useconds_t t;
    scanf("%d %u", &v, &t);

    int row = i/columns;
    cell_t* cell = cells + i;
    cell->value = v;
    cell->time = t;
    cell->row = matrix + row;
    cell->mutex = row_mutex + row;
    
    runnable.arg = cell;
    if((err = defer(&pool, runnable)) != 0)
      syserr(err, "Defer failed.");
  }

  //destroy wait for tasks to end
  thread_pool_destroy(&pool);
  
  for(int i = 0; i < rows; i++) {
    printf("%d\n", *(matrix + i));
  }

  free(matrix);
  free(row_mutex);
  free(cells);

  return 0;
}