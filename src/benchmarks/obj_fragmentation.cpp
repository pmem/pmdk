/*
 * Copyright 2018-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * obj_fragmentation.cpp -- fragmentation benchmarks definitions
 *
 * Benchmark has three scenarios "basic", "basic_with_peaks",
 * "basic_and_growth".
 * "Basic" runs the main operation.
 * "Basic_with_peaks" creates two workers. One performs the main operation,
 * the second worker runs peak operation.
 * "Basic_and_growth" creates two workers. One performs the main operation,
 * the second worker runs ramp operation.
 *
 * Each of the above scenarios is run an 'n' times, where 'n' - the number of
 * operations per thread.
 *
 * Type of the main operation depends on mem_usage_type. It can be flat,
 * ramp or peak.
 * Flat - there is only one allocation of memory,
 * Ramp - there is a set of allocation and deallocation of memory. Each
 * 	  subsequent allocation is larger than the previous one.
 * Peak - there is an allocation of a significant amount of memory or
 * 	  deallocation of allocated memory.
 */

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "benchmark_worker.hpp"
#include "file.h"
#include "libpmemobj.h"
#include "os.h"
#include "poolset_util.hpp"

/*
 * The factor used for PMEM pool size calculation, accounts for metadata,
 * fragmentation and etc.
 */
#define FACTOR 2

/* The minimum allocation size that pmalloc can perform */
#define ALLOC_MIN_SIZE 64

/* Size of the chunk using in calculations */
#define CHUNK 100

#define HEADER_SIZE 16

/*
 * mem_usage -- specifies how memory will grow
 */
enum mem_usage_type {
	MEM_USAGE_DEFAULT,
	MEM_USAGE_FLAT,
	MEM_USAGE_PEAK,
	MEM_USAGE_RAMP
};

/*
 * fragmentation - describes internal and external fragmentation
 * extern_fragmentation - describes only external fragmentation
 * extern_fragmentation_hdr - describes only external fragmentation and
 * takes account header size
 */
static float fragmentation = 0;
static float extern_fragmentation = 0;
static float extern_fragmentation_hdr = 0;

struct ramp_args {
	size_t growth_factor;     /* the amount by which the object size
				     will be increased in ramp mode */
	unsigned growth_interval; /* time after objects will grow
				     in ramp mode */
};

struct peak_args {
	size_t peak_multiplier; /* multipier of poolsize for peak
				   allocations */
	unsigned peak_lifetime; /* lifetime of peak allocations */
	unsigned peak_allocs;   /* number of peak allocations */
};
/*
 * prog_args -- benchmark specific arguments
 */
struct prog_args {
	char *background_mem_usage_type_str; /* mem_usage_type: flat, peak, ramp
					      */
	char *scenario;			     /* test scenario name */
	size_t start_obj_size;		     /* initial object size */
	size_t max_obj_size;		     /* maximum object size */
	unsigned operation_time;	     /* maximal operation time */
	unsigned seed;			     /* seed for randomization */
	bool use_random;		     /* use random numbers */
	ramp_args ramp;
	peak_args peak;
};

struct frag_obj {
	size_t block_size; /* size of pmemobj object */
	PMEMoid oid;
	bool is_allocated;
	int op_index;
	frag_obj(struct prog_args *args)
	{
		block_size = args->start_obj_size;
		oid = OID_NULL;
		is_allocated = false;
		op_index = -1;
	}
};

/*
 * frag_bench -- fragmentation benchmark context
 */
struct frag_bench {
	PMEMobjpool *pop;     /* persistent pool handle */
	struct prog_args *pa; /* prog_args structure */
	mem_usage_type background_mem_usage;
	uint64_t n_ops;
	size_t poolsize;
	size_t theoretical_memory_usage;
	struct frag_obj *
		*pmemobj_array; /* array of objects used in benchmark */
	int (*func_op)(frag_obj *op_obj, frag_bench *fb,
		       struct frag_worker *fworker);

	/* free all allocated pmemobj objects used in benchmark */
	void
	free_pmemobj_array(unsigned nb)
	{
		for (unsigned i = 0; i < nb; i++) {
			if (pmemobj_array[i]->is_allocated) {
				pmemobj_free(&pmemobj_array[i]->oid);
			}
			delete pmemobj_array[i];
		}
		delete[] pmemobj_array;
	}
};

/*
 * struct action_obj -- fragmentation benchmark action context
 */
struct action_obj {
	PMEMoid *peak_oids; /* oids array for peak allocations */
	bool peak_allocated;
	unsigned allocation_start_time; /* time of first allocation */
	unsigned deallocation_time;     /* time after which object should be
					   deallocated */
	unsigned next_action_time;      /* time of next action execution */

	/* function used in action execution */
	int (*action_op)(frag_obj *op_obj, frag_bench *fb,
			 struct frag_worker *worker);

	action_obj(struct frag_bench *fb)
	{
		peak_oids = (PMEMoid *)malloc(fb->pa->peak.peak_allocs *
					      sizeof(*peak_oids));
		peak_allocated = false;
		allocation_start_time = 0;
		deallocation_time = 0;
		next_action_time = 0;
	}

	~action_obj()
	{
		free(peak_oids);
	}
};

/*
 * struct frag_worker -- fragmentation worker context
 */
struct frag_worker {
	size_t op_obj_start_index;
	size_t max_block_size;
	size_t cur_block_size;
	size_t growth;
	unsigned current_test_time;
};

/*
 * struct scenario -- scenario name and function used in frag_operation function
 */
struct scenario {
	const char *scenario_name;
	int (*func_op)(frag_obj *op_obj, frag_bench *fb,
		       struct frag_worker *fworker);
};

/*
 * parse_memory_usage_type -- parse command line "--background-memory-usage"
 * argument
 *
 * Returns proper memory usage type.
 */
static mem_usage_type
parse_memory_usage_type(const char *arg)
{
	if (strcmp(arg, "flat") == 0)
		return MEM_USAGE_FLAT;
	if (strcmp(arg, "peak") == 0)
		return MEM_USAGE_PEAK;
	if (strcmp(arg, "ramp") == 0)
		return MEM_USAGE_RAMP;

	return MEM_USAGE_DEFAULT;
}

/*
 * alloc_obj -- allocate object and calculate theoretical memory usage
 *
 * Returns 0 when allocation succeeded, -1 otherwise.
 */
static int
alloc_obj(frag_obj *op_obj, frag_bench *fb, struct frag_worker *worker)
{
	if (pmemobj_alloc(fb->pop, &op_obj->oid, op_obj->block_size, 0, nullptr,
			  nullptr)) {
		perror("pmemobj_alloc");
		return -1;
	}
	op_obj->is_allocated = true;
	fb->theoretical_memory_usage += op_obj->block_size;
	return 0;
}

/*
 * dealloc_obj -- deallocates the memory indicated by op_obj.
 */
static void
dealloc_obj(frag_obj *op_obj, frag_bench *fb)
{
	if (op_obj->is_allocated) {
		pmemobj_free(&op_obj->oid);
		fb->theoretical_memory_usage -= op_obj->block_size;
		op_obj->is_allocated = false;
	}
}

/*
 * alloc_obj_if_not_allocated -- allocate object if it is not allocated
 * and calculate theoretical memory usage
 *
 * Returns 0 if the allocation is succeeded otherwise returns -1.
 */
static int
alloc_obj_if_not_allocated(frag_obj *op_obj, frag_bench *fb,
			   struct frag_worker *worker)
{
	if (!op_obj->is_allocated) {
		if (alloc_obj(op_obj, fb, worker)) {
			perror("alloc_obj");
			return -1;
		}
	}

	return 0;
}

/*
 * dealloc_peak -- deallocate objects allocated by alloc_peak function
 */
static void
dealloc_peak(unsigned peak_allocs, PMEMoid *oids)
{
	for (unsigned i = 0; i < (peak_allocs); ++i) {
		pmemobj_free(&oids[i]);
	}
}

/*
 * alloc_peak -- allocate multiple small objects to simulate peak memory usage
 *
 * Returns array of allocated objects oids.
 */
static int
alloc_peak(frag_bench *fb, PMEMoid *oids)
{
	for (unsigned i = 0; i < fb->pa->peak.peak_allocs; ++i) {
		if (pmemobj_alloc(fb->pop, &oids[i], ALLOC_MIN_SIZE, 0, nullptr,
				  nullptr)) {
			perror("pmemobj_alloc");
			if (i > 0)
				dealloc_peak(i - 1, oids);
			free(oids);
			return -1;
		}
	}

	return 0;
}

/*
 * dealloc_and_alloc_greater_obj -- if needed deallocate old object and allocate
 * bigger one
 *
 * Returns 0 if the allocation is succeeded otherwise returns -1.
 */
static int
dealloc_and_alloc_greater_obj(frag_obj *op_obj, frag_bench *fb,
			      struct frag_worker *worker)
{
	dealloc_obj(op_obj, fb);
	if (op_obj->block_size < worker->max_block_size) {
		op_obj->block_size += worker->growth;
		if (op_obj->block_size > worker->max_block_size) {
			op_obj->block_size = worker->max_block_size;
		}
	}

	return alloc_obj(op_obj, fb, worker);
}

/*
 * peak_action -- takes the required action for the peak scenario; deallocation
 * if the memory was previously allocated, allocation otherwise.
 *
 * Returns 0 if the action was performed successfully, -1 otherwise.
 */
static int
peak_action(frag_bench *fb, action_obj *action, int current_time)
{
	if (action->peak_allocated) {
		dealloc_peak(fb->pa->peak.peak_allocs, action->peak_oids);
		action->peak_allocated = false;
		action->next_action_time = action->deallocation_time;
	} else {
		if (alloc_peak(fb, action->peak_oids)) {
			perror("alloc_peak");
			return -1;
		}
		action->peak_allocated = true;
		unsigned optional_random_value = fb->pa->peak.peak_lifetime;

		if (fb->pa->use_random) {
			optional_random_value = os_rand_r(&fb->pa->seed) %
					fb->pa->peak.peak_lifetime +
				1;
		}
		action->next_action_time = current_time + optional_random_value;
	}

	return 0;
}

/*
 * worker_operation -- runs operation based on the selected scenario,
 * which is performed by a worker.
 *
 * Returns 0 if the action was performed successfully, -1 otherwise.
 */
static int
worker_operation(frag_obj *op_obj, frag_bench *fb, struct frag_worker *worker,
		 action_obj *action,
		 mem_usage_type mem_usage_type = MEM_USAGE_DEFAULT)
{
	unsigned current_time = worker->current_test_time;
	if (current_time != action->next_action_time &&
	    current_time != action->deallocation_time)
		return 0;

	if (mem_usage_type == MEM_USAGE_DEFAULT)
		mem_usage_type = fb->background_mem_usage;

	if (action->next_action_time != action->deallocation_time) {
		unsigned int optional_rand_value = 1;

		switch (mem_usage_type) {
			case MEM_USAGE_FLAT:
				break;
			case MEM_USAGE_RAMP:
				if (dealloc_and_alloc_greater_obj(op_obj, fb,
								  worker))
					return -1;

				optional_rand_value =
					fb->pa->ramp.growth_interval;

				if (fb->pa->use_random) {
					optional_rand_value =
						os_rand_r(&fb->pa->seed) %
							fb->pa->ramp
								.growth_interval +
						1;
				}

				action->next_action_time =
					current_time + optional_rand_value;
				break;
			case MEM_USAGE_PEAK:
				if (peak_action(fb, action, current_time))
					return -1;
				break;
			default:
				break;
		}

		if (action->next_action_time > action->deallocation_time) {
			action->next_action_time = action->deallocation_time;
		}
	} else {
		switch (mem_usage_type) {
			case MEM_USAGE_RAMP:
				dealloc_obj(op_obj, fb);
				break;
			case MEM_USAGE_PEAK:
				if (action->peak_allocated) {
					dealloc_peak(fb->pa->peak.peak_allocs,
						     action->peak_oids);
					action->peak_allocated = false;
				}
				break;
			default:
				break;
		}
	}

	return 0;
}

/*
 *optionally_random_value - returns value calculated randomly.
 */
static unsigned
optionally_random_value(unsigned val, frag_bench *fb)
{
	return val - (os_rand_r(&fb->pa->seed) % val) * fb->pa->use_random;
};

/*
 * init_basic_action -- function inits basic and/or worker's operation
 * in scenarios
 *
 * Returns 0 if action is properly initialized otherwise returns -1.
 */
static int
init_basic_action(frag_bench *fb, struct frag_worker *worker,
		  action_obj *action, mem_usage_type mem_usage_type)
{
	action->deallocation_time =
		optionally_random_value(fb->pa->operation_time, fb);
	switch (mem_usage_type) {
		case MEM_USAGE_FLAT:
			action->action_op = alloc_obj_if_not_allocated;
			action->next_action_time = action->deallocation_time;
			break;
		case MEM_USAGE_RAMP:
			action->action_op = dealloc_and_alloc_greater_obj;
			action->next_action_time =
				worker->current_test_time +
				optionally_random_value(
					fb->pa->ramp.growth_interval, fb);
			break;
		case MEM_USAGE_PEAK:
			action->action_op = alloc_obj_if_not_allocated;
			action->next_action_time =
				worker->current_test_time +
				optionally_random_value(
					fb->pa->peak.peak_lifetime, fb);
			break;
		default:
			return -1;
	}

	return 0;
}

/*
 * basic_op -- simplest scenario, runs only operation defined by
 * configuration(flat|peak|ramp)
 *
 * Returns 0 if scenario execution succeeded otherwise returns -1.
 */
static int
basic_op(frag_obj *op_obj, frag_bench *fb, struct frag_worker *worker)
{
	int ret = 0;
	action_obj *action = new action_obj(fb);
	if (!action)
		return -1;
	if (init_basic_action(fb, worker, action, fb->background_mem_usage)) {
		ret = -1;
		goto free_basic;
	}
	if (action->action_op(op_obj, fb, worker)) {
		ret = -1;
		goto free_basic;
	}
	while (worker->current_test_time < fb->pa->operation_time) {
		if (worker_operation(op_obj, fb, worker, action)) {
			ret = -1;
			goto free_basic;
		}
		worker->current_test_time++;
	}

free_basic:
	delete action;
	return ret;
}

/*
 * basic_with_peaks_op -- scenario runs operation defined by configuration
 * (flat|peak|ramp) and additional peak memory usage
 *
 * Returns 0 if scenario execution succeeded otherwise returns -1.
 */
static int
basic_with_peaks_op(frag_obj *op_obj, frag_bench *fb,
		    struct frag_worker *worker)
{
	int ret = 0;
	action_obj *basic_action = new action_obj(fb);
	if (!basic_action)
		return -1;
	action_obj *additional_peak = new action_obj(fb);
	if (!additional_peak) {
		ret = -1;
		goto free_basic_action;
	}

	if (init_basic_action(fb, worker, basic_action,
			      fb->background_mem_usage)) {
		ret = -1;
		goto free_additional_peak;
	}

	additional_peak->allocation_start_time =
		basic_action->allocation_start_time;
	additional_peak->next_action_time = basic_action->next_action_time;
	additional_peak->deallocation_time = basic_action->deallocation_time;

	if (basic_action->action_op(op_obj, fb, worker)) {
		ret = -1;
		goto free_additional_peak;
	}
	while (worker->current_test_time < fb->pa->operation_time) {
		if (worker_operation(op_obj, fb, worker, basic_action)) {
			ret = -1;
			goto free_additional_peak;
		}

		if (worker->current_test_time ==
			    additional_peak->next_action_time ||
		    worker->current_test_time ==
			    additional_peak->deallocation_time) {
			if (peak_action(fb, additional_peak,
					worker->current_test_time)) {
				ret = -1;
				goto free_additional_peak;
			}
			if (!additional_peak->peak_allocated)
				additional_peak->next_action_time =
					basic_action->next_action_time;
		}
		worker->current_test_time++;
	}

free_additional_peak:
	delete additional_peak;
free_basic_action:
	delete basic_action;
	return ret;
}

/*
 * basic_with_growth_op -- scenario runs operation defined by configuration
 * (flat|peak|ramp) and additional growing objects allocations
 *
 * Returns 0 if scenario execution succeeded otherwise returns -1.
 */
static int
basic_with_growth_op(frag_obj *op_obj, frag_bench *fb,
		     struct frag_worker *worker)
{
	int ret = 0;
	action_obj *basic_action = new action_obj(fb);
	if (!basic_action) {
		perror("malloc");
		return -1;
	}
	action_obj *additional_growth = new action_obj(fb);
	frag_obj *growth_obj = new frag_obj(fb->pa);
	if (additional_growth == nullptr) {
		perror("malloc");
		ret = -1;
		goto free_basic_action;
	}
	if (growth_obj == nullptr) {
		perror("malloc");
		ret = -1;
		goto free_additional_growth;
	}
	growth_obj->op_index = op_obj->op_index;

	if (init_basic_action(fb, worker, basic_action,
			      fb->background_mem_usage)) {
		ret = -1;
		goto free_growth_obj;
	}
	if (init_basic_action(fb, worker, additional_growth, MEM_USAGE_RAMP)) {
		ret = -1;
		goto free_growth_obj;
	}

	if (basic_action->action_op(op_obj, fb, worker)) {
		ret = -1;
		goto free_growth_obj;
	}
	if (additional_growth->action_op(growth_obj, fb, worker)) {
		ret = -1;
		goto free_growth_obj;
	}

	while (worker->current_test_time < fb->pa->operation_time) {
		if (worker_operation(op_obj, fb, worker, basic_action)) {
			ret = -1;
			goto free_growth_obj;
		}

		if (worker_operation(growth_obj, fb, worker, additional_growth,
				     MEM_USAGE_RAMP)) {
			ret = -1;
			goto free_growth_obj;
		}

		worker->current_test_time++;
	}
free_growth_obj:
	dealloc_obj(growth_obj, fb);
	delete growth_obj;
free_additional_growth:
	delete additional_growth;
free_basic_action:
	delete basic_action;
	return ret;
}

static struct scenario scenarios[] = {
	{"basic", basic_op}, // Memory usage as defined in configuration file
	{"basic_with_peaks",
	 basic_with_peaks_op}, // Additionally defined number of (de)allocations
	{"basic_and_growth",
	 basic_with_growth_op} // Additionally (de)allocation of
			       // increasing memory block
};

#define SCENARIOS_NUM ARRAY_SIZE(scenarios)

/*
 * parse_scenario -- parse command line "--scenario" argument
 *
 * Returns proper scenario name.
 */
static int
parse_scenario(const char *arg)
{
	for (unsigned i = 0; i < SCENARIOS_NUM; i++) {
		if (strcmp(arg, scenarios[i].scenario_name) == 0)
			return i;
	}

	return -1;
}

/*
 * frag_operation -- main operations for fragmentation benchmark
 */
static int
frag_operation(struct benchmark *bench, struct operation_info *info)
{
	auto *fb = (struct frag_bench *)pmembench_get_priv(bench);
	auto *fworker = (struct frag_worker *)info->worker->priv;
	auto *op_pmemobj =
		fb->pmemobj_array[fworker->op_obj_start_index + info->index];

	op_pmemobj->block_size = fworker->cur_block_size;
	op_pmemobj->op_index =
		(int)fworker->op_obj_start_index + (int)info->index;
	fworker->current_test_time = 0;

	return fb->func_op(op_pmemobj, fb, fworker);
}

/*
 * frag_init_worker -- init benchmark worker
 */
static int
frag_init_worker(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	frag_worker *fworker = new frag_worker();
	if (!fworker) {
		perror("malloc");
		return -1;
	}

	auto *fb = (struct frag_bench *)pmembench_get_priv(bench);

	fworker->op_obj_start_index = worker->index * args->n_ops_per_thread;
	fworker->cur_block_size = fb->pa->start_obj_size;
	fworker->max_block_size = fb->pa->max_obj_size;
	fworker->growth = fb->pa->ramp.growth_factor;
	fworker->current_test_time = 0;

	worker->priv = fworker;

	return 0;
}

/*
 * frag_free_worker -- cleanup benchmark worker
 */
static void
frag_free_worker(struct benchmark *bench, struct benchmark_args *args,
		 struct worker_info *worker)
{
	auto *fworker = (struct frag_worker *)worker->priv;
	delete fworker;
}

/*
 * frag_init -- benchmark initialization function
 */
static int
frag_init(struct benchmark *bench, struct benchmark_args *args)
{
	assert(bench != nullptr);
	assert(args != nullptr);
	assert(args->opts != nullptr);

	frag_bench *fb = new frag_bench();
	if (!fb) {
		perror("malloc");
		return -1;
	}

	fb->pa = (struct prog_args *)args->opts;
	assert(fb->pa != nullptr);
	assert(args->n_ops_per_thread != 0 && args->n_threads != 0);

	fb->n_ops = args->n_ops_per_thread;

	size_t n_objs = args->n_ops_per_thread * args->n_threads;
	fb->poolsize =
		n_objs * (fb->pa->max_obj_size * fb->pa->peak.peak_multiplier);

	fb->poolsize = PAGE_ALIGNED_UP_SIZE(fb->poolsize * FACTOR);
	long long object_size = -1;

	if (args->is_poolset ||
	    util_file_get_type(args->fname) == file_type::TYPE_DEVDAX) {
		if (args->fsize < fb->poolsize) {
			fprintf(stderr,
				"file size is smaller than required: %zu < %zu\n",
				args->fsize, fb->poolsize);
			goto free_fb;
		}
		fb->poolsize = args->fsize;
		object_size = 0;
	} else if (fb->poolsize < PMEMOBJ_MIN_POOL) {
		fb->poolsize = PMEMOBJ_MIN_POOL;
	}

	if (object_size < 0)
		object_size = fb->poolsize;
	fb->pop =
		pmemobj_create(args->fname, nullptr, object_size, args->fmode);
	if (fb->pop == nullptr) {
		fprintf(stderr, "pmemobj_create: %s\n", pmemobj_errormsg());
		goto free_fb;
	}

	fb->pmemobj_array = new frag_obj *[n_objs];
	if (fb->pmemobj_array == nullptr) {
		perror("malloc");
		goto free_pop;
	}
	for (size_t i = 0; i < n_objs; ++i) {
		fb->pmemobj_array[i] = new frag_obj(fb->pa);
		if (fb->pmemobj_array[i] == nullptr) {
			perror("malloc");
			if (i > 0)
				fb->free_pmemobj_array(unsigned(i - 1));
			goto free_pop;
		}
	}

	fb->background_mem_usage =
		parse_memory_usage_type(fb->pa->background_mem_usage_type_str);

	int scenario_index;
	scenario_index = parse_scenario(fb->pa->scenario);
	if (scenario_index == -1) {
		fprintf(stderr, "invalid scenario name: %s\n",
			fb->pa->scenario);
		goto free_pop;
	}
	fb->func_op = scenarios[scenario_index].func_op;
	fb->theoretical_memory_usage = 0;

	pmembench_set_priv(bench, fb);

	return 0;

free_pop:
	pmemobj_close(fb->pop);
free_fb:
	delete fb;
	return -1;
}

/*
 * frag_exit -- function for de-initialization benchmark
 */
static int
frag_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *fb = (struct frag_bench *)pmembench_get_priv(bench);

	size_t n_ops = args->n_ops_per_thread * args->n_threads;
	PMEMoid oid = OID_NULL;
	size_t remaining = 0;

	while (pmemobj_alloc(fb->pop, &oid, CHUNK, 0, NULL, NULL) == 0) {
		remaining += CHUNK + HEADER_SIZE;
	}

	size_t allocated_sum = 0;
	size_t allocated_sum2 = 0;
	oid = pmemobj_root(fb->pop, 1);
	for (size_t n = 0; n < n_ops; ++n) {
		if (fb->pmemobj_array[n]->is_allocated == false)
			continue;
		oid = fb->pmemobj_array[n]->oid;
		allocated_sum += pmemobj_alloc_usable_size(oid);
		allocated_sum2 += pmemobj_alloc_usable_size(oid) + HEADER_SIZE;
	}
	size_t used = fb->poolsize - remaining;

	fragmentation = (float)(used - fb->theoretical_memory_usage) / used;
	extern_fragmentation = (float)(used - allocated_sum) / used;
	extern_fragmentation_hdr = (float)(used - allocated_sum2) / used;

	printf("used = %zu\ntheoretical usage = %zu\npoolsize = %zu\nremaining = %zu\n",
	       used, fb->theoretical_memory_usage, fb->poolsize, remaining);

	fb->free_pmemobj_array((unsigned)n_ops);
	pmemobj_close(fb->pop);
	delete fb;

	return 0;
}

/*
 * frag_print_fragmentation -- function to print additional information
 */
static void
frag_print_fragmentation(struct benchmark *bench, struct benchmark_args *args,
			 struct total_results *res)
{
	printf("\n\nfragmentation(internal+external):\t%f\nfragmentation\
(external):\t\t%f\nfragmentation(external+header):\t\t%f",
	       fragmentation, extern_fragmentation, extern_fragmentation_hdr);
}

static struct benchmark_clo frag_clo[12];
static struct benchmark_info test_info;

CONSTRUCTOR(frag_constructor)
void
frag_constructor(void)
{
	frag_clo[0].opt_long = "background-memory-usage";
	frag_clo[0].descr = "Tested memory usage pattern (flat|peak|ramp)";
	frag_clo[0].type = CLO_TYPE_STR;
	frag_clo[0].off = clo_field_offset(struct prog_args,
					   background_mem_usage_type_str);
	frag_clo[0].def = "flat";
	frag_clo[0].ignore_in_res = false;

	frag_clo[1].opt_long = "start-obj-size";
	frag_clo[1].descr = "Initial object size";
	frag_clo[1].type = CLO_TYPE_UINT;
	frag_clo[1].off = clo_field_offset(struct prog_args, start_obj_size);
	frag_clo[1].def = "64";
	frag_clo[1].type_uint.size =
		clo_field_size(struct prog_args, start_obj_size);
	frag_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[1].type_uint.min = 0;
	frag_clo[1].type_uint.max = ~0;

	frag_clo[2].opt_long = "max-obj-size";
	frag_clo[2].descr = "Maximum object size";
	frag_clo[2].type = CLO_TYPE_UINT;
	frag_clo[2].off = clo_field_offset(struct prog_args, max_obj_size);
	frag_clo[2].def = "1024";
	frag_clo[2].type_uint.size =
		clo_field_size(struct prog_args, max_obj_size);
	frag_clo[2].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[2].type_uint.min = ALLOC_MIN_SIZE;
	frag_clo[2].type_uint.max = ~0;

	frag_clo[3].opt_long = "operation_time";
	frag_clo[3].descr = "Lifetime of object used in operation";
	frag_clo[3].type = CLO_TYPE_UINT;
	frag_clo[3].off = clo_field_offset(struct prog_args, operation_time);
	frag_clo[3].def = "1000";
	frag_clo[3].type_uint.size =
		clo_field_size(struct prog_args, operation_time);
	frag_clo[3].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[3].type_uint.min = 0;
	frag_clo[3].type_uint.max = ~0;

	frag_clo[4].opt_long = "peak-lifetime";
	frag_clo[4].descr = "Objects memory peak lifetime[ms]";
	frag_clo[4].type = CLO_TYPE_UINT;
	frag_clo[4].off =
		clo_field_offset(struct prog_args, peak.peak_lifetime);
	frag_clo[4].def = "10";
	frag_clo[4].type_uint.size =
		clo_field_size(struct prog_args, peak.peak_lifetime);
	frag_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[4].type_uint.min = 0;
	frag_clo[4].type_uint.max = ~0;

	frag_clo[5].opt_long = "growth";
	frag_clo[5].descr = "Amount by which the object size is increased";
	frag_clo[5].type = CLO_TYPE_UINT;
	frag_clo[5].off =
		clo_field_offset(struct prog_args, ramp.growth_factor);
	frag_clo[5].def = "8";
	frag_clo[5].type_uint.size =
		clo_field_size(struct prog_args, ramp.growth_factor);
	frag_clo[5].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[5].type_uint.min = 0;
	frag_clo[5].type_uint.max = ~0;

	frag_clo[6].opt_long = "peak-multiplier";
	frag_clo[6].descr = "Multiplier for peak memory usage growth";
	frag_clo[6].type = CLO_TYPE_UINT;
	frag_clo[6].off =
		clo_field_offset(struct prog_args, peak.peak_multiplier);
	frag_clo[6].def = "10";
	frag_clo[6].type_uint.size =
		clo_field_size(struct prog_args, peak.peak_multiplier);
	frag_clo[6].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[6].type_uint.min = 0;
	frag_clo[6].type_uint.max = ~0;

	frag_clo[7].opt_long = "peak-allocs";
	frag_clo[7].descr =
		"Number of (de)allocations to be performed in a time frame of benchmark";
	frag_clo[7].type = CLO_TYPE_UINT;
	frag_clo[7].off = clo_field_offset(struct prog_args, peak.peak_allocs);
	frag_clo[7].def = "100";
	frag_clo[7].type_uint.size =
		clo_field_size(struct prog_args, peak.peak_allocs);
	frag_clo[7].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[7].type_uint.min = 0;
	frag_clo[7].type_uint.max = ~0;

	frag_clo[8].opt_long = "scenario";
	frag_clo[8].descr =
		"Test scenario (basic|basic_with_peaks|basic_and_growth)";
	frag_clo[8].type = CLO_TYPE_STR;
	frag_clo[8].off = clo_field_offset(struct prog_args, scenario);
	frag_clo[8].def = "basic";

	frag_clo[9].opt_short = 'S';
	frag_clo[9].opt_long = "seed";
	frag_clo[9].descr = "Random seed";
	frag_clo[9].off = clo_field_offset(struct prog_args, seed);
	frag_clo[9].def = "1";
	frag_clo[9].type = CLO_TYPE_UINT;
	frag_clo[9].type_uint.size = clo_field_size(struct prog_args, seed);
	frag_clo[9].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[9].type_uint.min = 1;
	frag_clo[9].type_uint.max = UINT_MAX;

	frag_clo[10].opt_short = 'r';
	frag_clo[10].opt_long = "random";
	frag_clo[10].descr = "Use random operation times";
	frag_clo[10].off = clo_field_offset(struct prog_args, use_random);
	frag_clo[10].type = CLO_TYPE_FLAG;

	frag_clo[11].opt_long = "growth-interval";
	frag_clo[11].descr = "Time between growths";
	frag_clo[11].type = CLO_TYPE_UINT;
	frag_clo[11].off =
		clo_field_offset(struct prog_args, ramp.growth_interval);
	frag_clo[11].def = "100";
	frag_clo[11].type_uint.size =
		clo_field_size(struct prog_args, ramp.growth_interval);
	frag_clo[11].type_uint.base = CLO_INT_BASE_DEC;
	frag_clo[11].type_uint.min = 0;
	frag_clo[11].type_uint.max = ~0;

	test_info.name = "obj_fragmentation";
	test_info.brief = "Libpmemobj fragmentation benchmark";
	test_info.init = frag_init;
	test_info.exit = frag_exit;
	test_info.multithread = true;
	test_info.multiops = true;
	test_info.init_worker = frag_init_worker;
	test_info.free_worker = frag_free_worker;
	test_info.operation = frag_operation;
	test_info.print_extra_values = frag_print_fragmentation;
	test_info.clos = frag_clo;
	test_info.nclos = ARRAY_SIZE(frag_clo);
	test_info.opts_size = sizeof(struct prog_args);
	test_info.rm_file = true;
	test_info.allow_poolset = true;

	REGISTER_BENCHMARK(test_info);
}
