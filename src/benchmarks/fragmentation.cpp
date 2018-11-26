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
 * fragmentation.cpp -- fragmentation benchmarks definitions
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
/*
 * mem_usage -- specifies how memory will grow
 */
enum mem_usage_type {
  MEM_USAGE_DEFAULT,
  MEM_USAGE_FLAT,
  MEM_USAGE_PEAK,
  MEM_USAGE_RAMP
};

static float fragmentation = 0;
static float fragmentation2 = 0;
static float fragmentation3 = 0;

/*
 * prog_args -- benchmark specific arguments
 */
struct prog_args {
  char *background_mem_usage_type_str; /* mem_usage_type: flat, peak, ramp */
  char *scenario; /* test scenario name */
  size_t start_obj_size; /* initial object size */
  size_t max_obj_size; /* maximum object size */
  size_t growth; /* the amount by which the object size will be increased in ramp mode */
  size_t peak_multiplier; /* multipier of poolsize for peak allocations */
  unsigned growth_interval; /* time after objects will grow in ramp mode */
  unsigned operation_time; /* maximal operation time */
  unsigned peak_lifetime; /* lifetime of peak allocations */
  unsigned peak_allocs; /* number of peak allocations */
  unsigned seed; /* seed for randomization */
  bool rand; /* use random numbers */
};

struct frag_obj {
  size_t block_size; /*size of pmemobj*/
  PMEMoid oid;
  bool is_allocated;
  int op_index;

  void init(struct prog_args *args) {
    block_size = args->start_obj_size;
    is_allocated = false;
    op_index = -1;
  }
};

/*
 * frag_bench -- fragmentation benchmark context
 */
struct frag_bench {
  PMEMobjpool *pop; /* persistent pool handle */
  struct prog_args *pa; /* prog_args structure */
  mem_usage_type background_mem_usage;
  uint64_t n_ops;
  size_t poolsize;
  size_t theoretical_memory_usage;
  struct frag_obj **pmemobj_array; /*array of objects used in benchmark*/
  int(*func_op)(frag_obj * op_obj, frag_bench * fb,
                struct frag_worker * fworker);

  /*
   * free all allocated pmemobj objects used in benchmark
   */
  void free_pmemobj_array(unsigned nb) {
    for (unsigned i = 0; i < nb; i++) {
      if (pmemobj_array[i]->is_allocated) {
        pmemobj_free(&pmemobj_array[i]->oid);
      }
      free(pmemobj_array[i]);
    }
  }
};

/*
 * struct action_obj -- fragmentation benchmark action context
 */
struct action_obj {
  PMEMoid *peak_oids = nullptr; /*oids array for peak allocations*/
  bool peak_allocated = false;
  unsigned allocation_start_time = 0; /*time of first allocation*/
  unsigned deallocation_time = 0; /*time after object should be deallocated*/
  unsigned next_action_time = 0; /*time of next action execution*/

  /*function used in action execution*/
  int(*action_op)(frag_obj * op_obj, frag_bench * fb,
                  struct frag_worker * worker) = nullptr;
};

/*
 * struct frag_worker -- fragmentation worker context
 */
struct frag_worker {
  size_t op_obj_off;
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
  int(*func_op)(frag_obj * op_obj, frag_bench * fb,
                struct frag_worker * fworker);
};

/*
 * parse_memory_usage_type -- parse command line "--background-memory-usage"
 * argument
 *
 * Returns proper memory usage type.
 */
static mem_usage_type
parse_memory_usage_type(const char *arg) {
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
alloc_obj(frag_obj * op_obj, frag_bench * fb, struct frag_worker * worker) {
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
 * dealloc_obj -- Deallocates the memory indicated by op_obj.
 * Note that if op_obj point to the memory that was allocated the last,
 * the memory will not be deallocated because it needs to remain allocated
 * for the fragmentation calculation.
 */
static void
dealloc_obj(frag_obj * op_obj, int n_ops, size_t *mem_usage)
{
  if (op_obj->op_index < n_ops - 1) {
    pmemobj_free(&op_obj->oid);
    *mem_usage -= op_obj->block_size;
    op_obj->is_allocated = false;
  }
}

/*
 * alloc_obj_if_not_allocated -- allocate object if it is not alloated
 * and calculate theoretical memory usage
 *
 * Returns is allocation succeed.
 */
static int
alloc_obj_if_not_allocated(frag_obj * op_obj, frag_bench * fb,
                           struct frag_worker * worker) {
  if (!op_obj->is_allocated) {
    alloc_obj(op_obj, fb, worker);
  }
  return 0;
}

/*
 * alloc_peak -- allocate multiple small objects to simulate peak memory usage
 *
 * Returns array of allocated objects oids.
 */
static PMEMoid *alloc_peak(frag_bench *fb) {
  PMEMoid *oids = (PMEMoid *)malloc(fb->pa->peak_allocs * sizeof(*oids));

  for (unsigned i = 0; i <fb->pa->peak_allocs; ++i) {
    if (pmemobj_alloc(fb->pop, &oids[i], ALLOC_MIN_SIZE, 0, nullptr,
                      nullptr)) {
      perror("pmemobj_alloc");
      free(oids);
      return nullptr;
    }
  }
  return oids;
}

/*
 * dealloc_peak -- deallocate objects allocated by alloc_peak function
 */
static void
dealloc_peak(frag_bench *fb, PMEMoid *oids) {
  for (unsigned i = 0; i < fb->pa->peak_allocs; ++i) {
    pmemobj_free(&oids[i]);
  }

  free(oids);
}

/*
 * alloc_greater_obj -- if needed deallocate old object and allocate bigger one
 *
 * Returns is allocation succeed.
 */
static int
alloc_greater_obj(frag_obj * op_obj, frag_bench * fb,
  struct frag_worker * worker) {
  if(op_obj->is_allocated)
    dealloc_obj(op_obj, (int) fb->n_ops, &fb->theoretical_memory_usage);
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
 * if the memory was previously allocated, allocation otherwis.
 *
 * Returns 0 is the action was performed successfully, -1 otherwise.
 */
static int
peak_action(frag_bench * fb, action_obj * action, int current_time) {
  if (action->peak_allocated) {
    dealloc_peak(fb, action->peak_oids);
    action->peak_allocated = false;
    action->next_action_time = action->deallocation_time;
  } else {
    action->peak_oids = alloc_peak(fb);
    if (action->peak_oids == nullptr)
      return -1;
    action->peak_allocated = true;
    unsigned next_action = fb->pa->peak_lifetime;

    if (fb->pa->rand) {
      next_action = os_rand_r(&fb->pa->seed) % fb->pa->peak_lifetime + 1;
    }
    action->next_action_time = current_time + next_action;
  }
  return 0;
}

/*
 * background_operation -- performs the required action based on the selected
 * scenario.
 *
 * Returns 0 if the action was performed successfully, -1 otherwise.
 */
static int
background_operation(frag_obj * op_obj, frag_bench * fb,
                     struct frag_worker * worker,
                     action_obj * action,
                     mem_usage_type mem_usage_type = MEM_USAGE_DEFAULT) {
  unsigned current_time = worker->current_test_time;
  if (current_time == action->next_action_time ||
      current_time == action->deallocation_time) {
    if (mem_usage_type == MEM_USAGE_DEFAULT)
      mem_usage_type = fb->background_mem_usage;

    if (action->next_action_time != action->deallocation_time) {
      unsigned int next_action = 1;

      switch (mem_usage_type) {
        case MEM_USAGE_FLAT:
          break;
        case MEM_USAGE_RAMP:
          if (alloc_greater_obj(op_obj, fb, worker))
            return -1;

          next_action = fb->pa->growth_interval;

          if (fb->pa->rand) {
            next_action = os_rand_r(&fb->pa->seed) %
              fb->pa->growth_interval + 1;
          }

          action->next_action_time = current_time + next_action;
          break;
        case MEM_USAGE_PEAK:
          peak_action(fb, action, current_time);
          break;
        default:
          break;
      }

      if (action->next_action_time > action->deallocation_time) {
        action->next_action_time = action->deallocation_time;
      }
    } else {
      dealloc_obj(op_obj, (int) fb->n_ops, &fb->theoretical_memory_usage);
    }
  }
  return 0;
}

/*
 * background_operation -- function maintain basic and/or background operation
 * in scenarios
 *
 * Returns is action is proper initialized.
 */
static int
init_basic_action(frag_bench * fb, struct frag_worker * worker,
                  action_obj * action, mem_usage_type mem_usage_type) {
  action->deallocation_time = fb->pa->operation_time -
    (os_rand_r(&fb->pa->seed) % fb->pa->operation_time) *
    fb->pa->rand;

  switch (mem_usage_type) {
    case MEM_USAGE_FLAT:
      action->action_op = alloc_obj_if_not_allocated;
      action->allocation_start_time = 0;
      action->next_action_time = action->deallocation_time;
      break;
    case MEM_USAGE_RAMP:
      action->action_op = alloc_greater_obj;
      action->allocation_start_time = 0;
      action->next_action_time = worker->current_test_time +
        fb->pa->growth_interval -
        (os_rand_r(&fb->pa->seed) % fb->pa->growth_interval) *
        fb->pa->rand;
      break;
    case MEM_USAGE_PEAK:
      action->action_op = alloc_obj_if_not_allocated;
      action->allocation_start_time = 0;
      action->next_action_time = worker->current_test_time +
        fb->pa->peak_lifetime -
        (os_rand_r(&fb->pa->seed) % fb->pa->peak_lifetime) *
        fb->pa->rand;
      break;
    default:
      return -1;
  }
  return 0;
}

/*
 * basic -- simplest scenario
 *
 * Returns is scenario execution succeed.
 */
static int
basic(frag_obj * op_obj, frag_bench * fb, struct frag_worker * worker) {
  auto *action = (struct action_obj *)malloc(sizeof(struct action_obj));

  if (init_basic_action(fb, worker, action, fb->background_mem_usage))
    goto err;

  if (action->action_op(op_obj, fb, worker))
    goto err;

  while (worker->current_test_time<fb->pa->operation_time) {
    if (background_operation(op_obj, fb, worker, action))
      goto err;
    worker->current_test_time++;
  }
  free(action);
  return 0;
err:
  free(action);
  return -1;
}

/*
 * add_peaks -- scenario with basic scenario as background and additional peak
 * memory usage
 *
 * Returns is scenario execution succeed.
 */
static int
add_peaks(frag_obj * op_obj, frag_bench * fb, struct frag_worker * worker) {
  auto *basic_action =
    (struct action_obj *)malloc(sizeof(struct action_obj));
  if (basic_action == nullptr) {
    perror("malloc");
    return -1;
  }

  auto *additional_peak =
    (struct action_obj *)malloc(sizeof(struct action_obj));
  if (additional_peak == nullptr) {
    free(basic_action);
    perror("malloc");
    return -1;
  }

  if (init_basic_action(fb, worker, basic_action, fb->background_mem_usage))
    goto err;

  additional_peak->allocation_start_time = basic_action->next_action_time;
  additional_peak->next_action_time = basic_action->next_action_time;
  additional_peak->deallocation_time = basic_action->deallocation_time;

  if (basic_action->action_op(op_obj, fb, worker))
    goto err;

  while (worker->current_test_time<fb->pa->operation_time) {
    if (background_operation(op_obj, fb, worker, basic_action))
      goto err;

    if (worker->current_test_time == additional_peak->next_action_time ||
        worker->current_test_time == additional_peak->deallocation_time) {
      peak_action(fb, additional_peak, worker->current_test_time);
      if (!additional_peak->peak_allocated)
        additional_peak->next_action_time = basic_action->next_action_time;
    }
    worker->current_test_time++;
#ifndef _WIN32
    usleep(1);
#else
    Sleep(1);
#endif
  }
  free(basic_action);
  free(additional_peak);
  return 0;
err:
  free(basic_action);
  free(additional_peak);
  return -1;
}

/*
 * basic_and_growth -- scenario with basic scenario as background and additional
 * growing objects allocations
 *
 * Returns is scenario execution succeed.
 */
static int
basic_and_growth(frag_obj * op_obj, frag_bench * fb,
                 struct frag_worker * worker) {
  auto *basic_action = (struct action_obj *)
    malloc(sizeof(struct action_obj));
  if (basic_action == nullptr) {
    perror("malloc");
    return -1;
  }
  auto *additional_growth = (struct action_obj *)malloc(sizeof(struct action_obj));
  if (additional_growth == nullptr) {
    free(basic_action);
    perror("malloc");
    return -1;
  }
  auto * growth_obj = (struct frag_obj*)malloc(sizeof(struct frag_obj));
  if (growth_obj == nullptr) {
    perror("malloc");
    goto err;
  }
  growth_obj->init(fb->pa);

  if (init_basic_action(fb, worker, basic_action, fb->background_mem_usage))
    goto err;
  if (init_basic_action(fb, worker, additional_growth, MEM_USAGE_RAMP))
    goto err;

  if (basic_action->action_op(op_obj, fb, worker))
    goto err;
  if (additional_growth->action_op(growth_obj, fb, worker))
    goto err;

  while (worker->current_test_time<fb->pa->operation_time) {
    if (background_operation(op_obj, fb, worker, basic_action))
      goto err;

    if (background_operation(growth_obj, fb, worker, additional_growth,
                             MEM_USAGE_RAMP))
      goto err;

    worker->current_test_time++;
#ifndef _WIN32
    usleep(1);
#else
    Sleep(1);
#endif
  }

  free(growth_obj);
  free(additional_growth);
  free(basic_action);
  return 0;
err:
  free(growth_obj);
  free(additional_growth);
  free(basic_action);
  return -1;
}

static struct scenario scenarios[] = {
  { "basic", basic },                       // Memory usage as definted in configuration file
  { "basic_with_peaks", add_peaks },        // Additionaaly defined number of (de)allocations
  { "basic_and_growth", basic_and_growth }  // Additionally (de)allocation of increasing memory block
};

#define SCENARIOS (sizeof(scenarios) / sizeof(scenarios[0]))

/*
 * parse_scenario -- parse command line "--scenario" argument
 *
 * Returns proper scenario name.
 */
static int parse_scenario(const char *arg) {
  for (unsigned i = 0; i < SCENARIOS; i++) {
    if (strcmp(arg, scenarios[i].scenario_name) == 0)
      return i;
  }
  return -1;
}

/*
 * frag_operation -- main operations for fragmentation benchmark
 */
static int
frag_operation(struct benchmark *bench, struct operation_info *info) {

  printf("/nfrag_operation/n");

  auto *fb = (struct frag_bench *)pmembench_get_priv(bench);
  auto *fworker = (struct frag_worker *)info->worker->priv;
  auto *op_pmemobj = fb->pmemobj_array[fworker->op_obj_off + info->index];

  op_pmemobj->block_size = fworker->cur_block_size;
  op_pmemobj->op_index = (int) fworker->op_obj_off + (int)info->index;
  fworker->current_test_time = 0;

  return fb->func_op(op_pmemobj, fb, fworker);
}

/*
 * frag_init_worker -- init benchmark worker
 */
static int
frag_init_worker(struct benchmark *bench,
                 struct benchmark_args *args,
                 struct worker_info *worker) {
  struct frag_worker *fworker = (struct frag_worker *)malloc(sizeof(*fworker));

  if (!fworker) {
    perror("malloc");
    return -1;
  }

  auto *fb = (struct frag_bench *)pmembench_get_priv(bench);

  fworker->op_obj_off = worker->index * args->n_ops_per_thread;
  fworker->cur_block_size = fb->pa->start_obj_size;
  fworker->max_block_size = fb->pa->max_obj_size;
  fworker->growth = fb->pa->growth;
  fworker->current_test_time = 0;

  worker->priv = fworker;
  return 0;
}

/*
 * frag_free_worker -- cleanup benchmark worker
 */
static void
frag_free_worker(struct benchmark *bench,
                 struct benchmark_args *args,
                 struct worker_info *worker) {
  auto *fworker = (struct frag_worker *)worker->priv;
  free(fworker);
}

/*
 * frag_init -- benchmark initialization function
 */
static int
frag_init(struct benchmark *bench, struct benchmark_args *args) {
  assert(bench != nullptr);
  assert(args != nullptr);
  assert(args->opts != nullptr);

  int scenario_index;
  auto *fa = (struct prog_args *)args->opts;
  assert(fa != nullptr);

  auto *fb = (struct frag_bench *)malloc(sizeof(struct frag_bench));
  if (fb == nullptr) {
    perror("malloc");
    return -1;
  }

  fb->pa = (struct prog_args *)args->opts;

  size_t n_ops_total = args->n_ops_per_thread * args->n_threads;
  assert(n_ops_total != 0);

  fb->n_ops = args->n_ops_per_thread;

  /* Create pmemobj pool. */
  if (fb->pa->max_obj_size < ALLOC_MIN_SIZE)
    fb->pa->max_obj_size = ALLOC_MIN_SIZE;

  /* For data objects */
  size_t n_objs = args->n_ops_per_thread * args->n_threads;
  fb->poolsize = n_objs * (fb->pa->max_obj_size * fb->pa->peak_multiplier);

  /* multiply by FACTOR for metadata, fragmentation, etc. */
  fb->poolsize = fb->poolsize * FACTOR;

  if (args->is_poolset || util_file_get_type(args->fname)==file_type::TYPE_DEVDAX) {
    if (args->fsize < fb->poolsize) {
      fprintf(stderr, "file size too large\n");
      goto free_fb;
    }
    fb->poolsize = 0;
  }
  else if (fb->poolsize < PMEMOBJ_MIN_POOL) {
    fb->poolsize = PMEMOBJ_MIN_POOL;
  }

  fb->poolsize = PAGE_ALIGNED_UP_SIZE(fb->poolsize);

  fb->pop = pmemobj_create(args->fname, nullptr, fb->poolsize, args->fmode);
  if (fb->pop == nullptr) {
    fprintf(stderr, "%s\n", pmemobj_errormsg());
    goto free_fb;
  }

  fb->pmemobj_array = (struct frag_obj **)malloc(sizeof(struct frag_obj*) *
    n_objs);
  if (fb->pmemobj_array == nullptr) {
    perror("malloc");
    goto free_pop;
  }
  for (unsigned i = 0; i < n_objs; ++i) {
    fb->pmemobj_array[i]= (struct frag_obj *)
      malloc(sizeof(struct frag_obj));

    if (fb->pmemobj_array[i] == nullptr) {
      perror("malloc");
      if(i > 0) {
        fb->free_pmemobj_array(i - 1);
      }
      goto free_pop;
    }
    fb->pmemobj_array[i]->init(fb->pa);
  }

  fb->background_mem_usage =
    parse_memory_usage_type(fb->pa->background_mem_usage_type_str);
  scenario_index = parse_scenario(fb->pa->scenario);
  if (scenario_index == -1) {
    perror("wrong scenario name");
    goto free_pop;
  }
  fb->func_op = scenarios[scenario_index].func_op;
  fb->theoretical_memory_usage = 0;

  pmembench_set_priv(bench, fb);
  fragmentation = 0;
  return 0;

free_pop:
  pmemobj_close(fb->pop);
free_fb:
  free(fb);
  return -1;
}

/*
 * frag_exit -- function for de-initialization benchmark
 */
static int
frag_exit(struct benchmark *bench, struct benchmark_args *args)
{
  auto *fb = (struct frag_bench *)pmembench_get_priv(bench);

  size_t n_ops = args->n_ops_per_thread *args->n_threads;
  PMEMoid oid;
  size_t remaining = 0;
  size_t chunk = 100; /* calc at chunk level */
  while (pmemobj_alloc(fb->pop, &oid, chunk, 0, NULL, NULL) == 0) {
    remaining += pmemobj_alloc_usable_size(oid) + 16;
  }

  size_t allocated_sum = 0;
  size_t allocated_sum2 = 0;
  oid = pmemobj_root(fb->pop, 1);
  for (size_t n = 0; n < n_ops; ++n) {
    if (fb->pmemobj_array[n]->is_allocated == false)
      continue;
    oid = fb->pmemobj_array[n]->oid;
    oid.pool_uuid_lo = fb->pmemobj_array[n]->oid.pool_uuid_lo;
    allocated_sum += pmemobj_alloc_usable_size(oid);
    allocated_sum2 += pmemobj_alloc_usable_size(oid) +16;
  }

  size_t used = fb->poolsize - remaining;

  fragmentation = (float)(used - fb->theoretical_memory_usage) / used;
  fragmentation2 = (float)(used - allocated_sum) / used;
  fragmentation3 = (float)(used - allocated_sum2) / used;

  fb->free_pmemobj_array((unsigned) n_ops);
  pmemobj_close(fb->pop);
  free(fb);

  printf ("used = %zu\ntheoretical usage = %zu\npoolsize = %zu\nremaining = %zu\n", used, fb->theoretical_memory_usage, fb->poolsize, remaining);
  return 0;
}

/*
 * frag_print_fragmentation -- function to print additional information
 */
static void
frag_print_fragmentation(struct benchmark *bench,
                         struct benchmark_args *args,
                         struct total_results *res) {
  printf("\nfragmentation:\t%f\t%f\t%f", fragmentation, fragmentation2,
         fragmentation3);
}

static struct benchmark_clo frag_clo[12];
static struct benchmark_info test_info;

CONSTRUCTOR(frag_constructor)
void frag_constructor(void) {
  frag_clo[0].opt_long = "background-memory-usage";
  frag_clo[0].descr = "Tested memory usage pattern";
  frag_clo[0].type = CLO_TYPE_STR;
  frag_clo[0].off = clo_field_offset(struct prog_args, background_mem_usage_type_str);
  frag_clo[0].def = "flat";
  frag_clo[0].ignore_in_res = false;

  frag_clo[1].opt_long = "start-obj-size";
  frag_clo[1].descr = "start obj size";
  frag_clo[1].type = CLO_TYPE_UINT;
  frag_clo[1].off = clo_field_offset(struct prog_args, start_obj_size);
  frag_clo[1].def = "64";
  frag_clo[1].type_uint.size = clo_field_size(struct prog_args, start_obj_size);
  frag_clo[1].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[1].type_uint.min = 0;
  frag_clo[1].type_uint.max = ~0;

  frag_clo[2].opt_long = "max-obj-size";
  frag_clo[2].descr = "maximum obj size";
  frag_clo[2].type = CLO_TYPE_UINT;
  frag_clo[2].off = clo_field_offset(struct prog_args, max_obj_size);
  frag_clo[2].def = "1024";
  frag_clo[2].type_uint.size = clo_field_size(struct prog_args, max_obj_size);
  frag_clo[2].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[2].type_uint.min = 0;
  frag_clo[2].type_uint.max = ~0;

  frag_clo[3].opt_long = "operation_time";
  frag_clo[3].descr = "lifetime of object used in operation";
  frag_clo[3].type = CLO_TYPE_UINT;
  frag_clo[3].off = clo_field_offset(struct prog_args, operation_time);
  frag_clo[3].def = "1000";
  frag_clo[3].type_uint.size = clo_field_size(struct prog_args, operation_time);
  frag_clo[3].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[3].type_uint.min = 0;
  frag_clo[3].type_uint.max = ~0;

  frag_clo[4].opt_long = "peak-lifetime";
  frag_clo[4].descr = "objects memory peak lifetime in ms";
  frag_clo[4].type = CLO_TYPE_UINT;
  frag_clo[4].off = clo_field_offset(struct prog_args, peak_lifetime);
  frag_clo[4].def = "10";
  frag_clo[4].type_uint.size = clo_field_size(struct prog_args, peak_lifetime);
  frag_clo[4].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[4].type_uint.min = 0;
  frag_clo[4].type_uint.max = ~0;

  frag_clo[5].opt_long = "growth";
  frag_clo[5].descr = "amount by which the object size is increased";
  frag_clo[5].type = CLO_TYPE_UINT;
  frag_clo[5].off = clo_field_offset(struct prog_args, growth);
  frag_clo[5].def = "8";
  frag_clo[5].type_uint.size = clo_field_size(struct prog_args, growth);
  frag_clo[5].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[5].type_uint.min = 0;
  frag_clo[5].type_uint.max = ~0;

  frag_clo[6].opt_long = "peak-multiplier";
  frag_clo[6].descr = "multiplier for peak memory usage growth";
  frag_clo[6].type = CLO_TYPE_UINT;
  frag_clo[6].off = clo_field_offset(struct prog_args, peak_multiplier);
  frag_clo[6].def = "10";
  frag_clo[6].type_uint.size = clo_field_size(struct prog_args, peak_multiplier);
  frag_clo[6].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[6].type_uint.min = 0;
  frag_clo[6].type_uint.max = ~0;

  frag_clo[7].opt_long = "peak-allocs";
  frag_clo[7].descr = "number of (de)allocations to be performed in a short time frame";
  frag_clo[7].type = CLO_TYPE_UINT;
  frag_clo[7].off = clo_field_offset(struct prog_args, peak_allocs);
  frag_clo[7].def = "100";
  frag_clo[7].type_uint.size = clo_field_size(struct prog_args, peak_allocs);
  frag_clo[7].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[7].type_uint.min = 0;
  frag_clo[7].type_uint.max = ~0;

  frag_clo[8].opt_long = "scenario";
  frag_clo[8].descr = "test scenario";
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
  frag_clo[10].off = clo_field_offset(struct prog_args, rand);
  frag_clo[10].type = CLO_TYPE_FLAG;

  frag_clo[11].opt_long = "growth-interval";
  frag_clo[11].descr = "time after object will grow";
  frag_clo[11].type = CLO_TYPE_UINT;
  frag_clo[11].off = clo_field_offset(struct prog_args, growth_interval);
  frag_clo[11].def = "100";
  frag_clo[11].type_uint.size = clo_field_size(struct prog_args, growth_interval);
  frag_clo[11].type_uint.base = CLO_INT_BASE_DEC;
  frag_clo[11].type_uint.min = 0;
  frag_clo[11].type_uint.max = ~0;

  test_info.name = "test_frag";
  test_info.brief = "Benchmark test_frag operation";
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
