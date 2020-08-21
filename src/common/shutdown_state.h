/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * shutdown_state.h -- unsafe shudown detection
 */

#ifndef PMDK_SHUTDOWN_STATE_H
#define PMDK_SHUTDOWN_STATE_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pool_replica;
struct shutdown_state {
	uint64_t usc;
	uint64_t uuid; /* UID checksum */
	uint8_t dirty;
	uint8_t reserved[39];
	uint64_t checksum;
};

int shutdown_state_init(struct shutdown_state *sds, struct pool_replica *rep);
int shutdown_state_add_part(struct shutdown_state *sds, int fd,
	struct pool_replica *rep);
void shutdown_state_set_dirty(struct shutdown_state *sds,
	struct pool_replica *rep);
void shutdown_state_clear_dirty(struct shutdown_state *sds,
	struct pool_replica *rep);

int shutdown_state_check(struct shutdown_state *curr_sds,
	struct shutdown_state *pool_sds, struct pool_replica *rep);

#ifdef __cplusplus
}
#endif

#endif /* shutdown_state.h */
