/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * lane.h -- internal definitions for lanes
 */

#ifndef LIBPMEMOBJ_LANE_H
#define LIBPMEMOBJ_LANE_H 1

#include <stdint.h>
#include "ulog.h"
#include "libpmemobj.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Distance between lanes used by threads required to prevent threads from
 * false sharing part of lanes array. Used if properly spread lanes are
 * available. Otherwise less spread out lanes would be used.
 */
#define LANE_JUMP (64 / sizeof(uint64_t))

/*
 * Number of times the algorithm will try to reacquire the primary lane for the
 * thread. If this threshold is exceeded, a new primary lane is selected for the
 * thread.
 */
#define LANE_PRIMARY_ATTEMPTS 128

#define RLANE_DEFAULT 0

#define LANE_TOTAL_SIZE 3072 /* 3 * 1024 (sum of 3 old lane sections) */
/*
 * We have 3 kilobytes to distribute.
 * The smallest capacity is needed for the internal redo log for which we can
 * accurately calculate the maximum number of occupied space: 48 bytes,
 * 3 times sizeof(struct ulog_entry_val). One for bitmap OR, second for bitmap
 * AND, third for modification of the destination pointer. For future needs,
 * this has been bumped up to 12 ulog entries.
 *
 * The remaining part has to be split between transactional redo and undo logs,
 * and since by far the most space consuming operations are transactional
 * snapshots, most of the space, 2 kilobytes, is assigned to the undo log.
 * After that, the remainder, 640 bytes, or 40 ulog entries, is left for the
 * transactional redo logs.
 * Thanks to this distribution, all small and medium transactions should be
 * entirely performed without allocating any additional metadata.
 *
 * These values must be cacheline size aligned to be used for ulogs. Therefore
 * they are parametrized for the size of the struct ulog changes between
 * platforms.
 */
#define LANE_UNDO_SIZE (LANE_TOTAL_SIZE \
			- LANE_REDO_EXTERNAL_SIZE \
			- LANE_REDO_INTERNAL_SIZE \
			- 3 * sizeof(struct ulog)) /* 2048 for 64B ulog */
#define LANE_REDO_EXTERNAL_SIZE ALIGN_UP(704 - sizeof(struct ulog), \
					CACHELINE_SIZE) /* 640 for 64B ulog */
#define LANE_REDO_INTERNAL_SIZE ALIGN_UP(256 - sizeof(struct ulog), \
					CACHELINE_SIZE) /* 192 for 64B ulog */

struct lane_layout {
	/*
	 * Redo log for self-contained and 'one-shot' allocator operations.
	 * Cannot be extended.
	 */
	struct ULOG(LANE_REDO_INTERNAL_SIZE) internal;
	/*
	 * Redo log for large operations/transactions.
	 * Can be extended by the use of internal ulog.
	 */
	struct ULOG(LANE_REDO_EXTERNAL_SIZE) external;
	/*
	 * Undo log for snapshots done in a transaction.
	 * Can be extended/shrunk by the use of internal ulog.
	 */
	struct ULOG(LANE_UNDO_SIZE) undo;
};

struct lane {
	struct lane_layout *layout; /* pointer to persistent layout */
	struct operation_context *internal; /* context for internal ulog */
	struct operation_context *external; /* context for external ulog */
	struct operation_context *undo; /* context for undo ulog */
};

struct lane_descriptor {
	/*
	 * Number of lanes available at runtime must be <= total number of lanes
	 * available in the pool. Number of lanes can be limited by shortage of
	 * other resources e.g. available RNIC's submission queue sizes.
	 */
	unsigned runtime_nlanes;
	unsigned next_lane_idx;
	uint64_t *lane_locks;
	struct lane *lane;
};

typedef int (*section_layout_op)(PMEMobjpool *pop, void *data, unsigned length);
typedef void *(*section_constr)(PMEMobjpool *pop, void *data);
typedef void (*section_destr)(PMEMobjpool *pop, void *rt);
typedef int (*section_global_op)(PMEMobjpool *pop);

struct section_operations {
	section_constr construct_rt;
	section_destr destroy_rt;
	section_layout_op check;
	section_layout_op recover;
	section_global_op boot;
	section_global_op cleanup;
};

struct lane_info {
	uint64_t pop_uuid_lo;
	uint64_t lane_idx;
	unsigned long nest_count;

	/*
	 * The index of the primary lane for the thread. A thread will always
	 * try to acquire the primary lane first, and only if that fails it will
	 * look for a different available lane.
	 */
	uint64_t primary;
	int primary_attempts;

	struct lane_info *prev, *next;
};

void lane_info_boot(void);
void lane_info_destroy(void);

void lane_init_data(PMEMobjpool *pop);
int lane_boot(PMEMobjpool *pop);
void lane_cleanup(PMEMobjpool *pop);
int lane_recover_and_section_boot(PMEMobjpool *pop);
int lane_section_cleanup(PMEMobjpool *pop);
int lane_check(PMEMobjpool *pop);

unsigned lane_hold(PMEMobjpool *pop, struct lane **lane);
void lane_release(PMEMobjpool *pop);

#ifdef __cplusplus
}
#endif

#endif
