/*
 * Copyright 2017-2018, Intel Corporation
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
int shutdown_state_add_part(struct shutdown_state *sds, const char *path,
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
