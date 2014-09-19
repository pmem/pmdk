#ifndef TASKS_H
#define	TASKS_H

#include <stdlib.h>
#include <inttypes.h>
#include <libvmem.h>

#define	SUCCESS 0
#define	FAILURE -1

typedef enum allocator_e {
	ALLOCATOR_VMEM,
	ALLOCATOR_MALLOC,
	MAX_ALLOCATOR
} allocator_t;

typedef enum allocation_type_e {
	ALLOCATION_UNKNOWN,
	ALLOCATION_STATIC,
	ALLOCATION_RANGE
} allocation_type_t;

typedef struct arguments_s
{
	int thread_count;
	int pool_per_thread;
	uint64_t ops_count;
	uint_t seed;
	uint_t allocation_size;
	uint_t allocation_size_max;
	allocation_type_t allocation_type;
	allocator_t allocator;
} arguments_t;

typedef int (*task_f)(int, void *arg, struct random_data *rand_state);

enum {
	TASK_MALLOC,
	TASK_FREE,
	MAX_TASK
};

int task_malloc(int i, void *arg, struct random_data *rand_state);
int task_free(int i, void *arg, struct random_data *rand_state);

extern task_f tasks[MAX_TASK];

extern int allocation_range_min;
extern int allocation_range_max;
extern allocator_t allocator;

extern void **allocated_mem;

extern const int allocation_sizes[];

int run_threads(arguments_t *arguments, task_f task,
	int per_thread_arg, void **arg, double *elapsed);

#endif // TASKS_H
