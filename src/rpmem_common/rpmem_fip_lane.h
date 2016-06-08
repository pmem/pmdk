/*
 * Copyright 2016, Intel Corporation
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
 * rpmem_fip_lane.h -- rpmem fabric provider lane definition
 */

#include <sched.h>
#include <stdint.h>

/*
 * rpmem_fip_lane -- basic lane structure
 *
 * This structure consist of a synchronization object and a return value.
 * It is possible to wait on the lane for specified event. The event can be
 * signalled by another thread which can pass the return value if required.
 *
 * The sync variable can store up to 64 different events, each event on
 * separate bit.
 */
struct rpmem_fip_lane {
	volatile int ret;
	volatile uint64_t sync;
};

/*
 * rpmem_fip_lane_init -- initialize basic lane structure
 */
static inline int
rpmem_fip_lane_init(struct rpmem_fip_lane *lanep)
{
	lanep->ret = 0;
	lanep->sync = 0;

	return 0;
}

/*
 * rpmem_fip_lane_fini -- deinitialize basic lane structure
 */
static inline void
rpmem_fip_lane_fini(struct rpmem_fip_lane *lanep)
{
	/* nothing to do */
}

/*
 * rpmem_fip_lane_busy -- return true if lane has pending events
 */
static inline int
rpmem_fip_lane_busy(struct rpmem_fip_lane *lanep)
{
	return lanep->sync != 0;
}

/*
 * rpmem_fip_lane_begin -- begin waiting for specified event(s)
 */
static inline void
rpmem_fip_lane_begin(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	lanep->ret = 0;
	__sync_fetch_and_or(&lanep->sync, sig);
}

/*
 * rpmem_fip_lane_wait -- wait for specified event(s)
 */
static inline int
rpmem_fip_lane_wait(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	while (lanep->sync & sig)
		sched_yield();

	return lanep->ret;
}

/*
 * rpmem_fip_lane_signal -- signal lane about specified event
 */
static inline void
rpmem_fip_lane_signal(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	__sync_fetch_and_and(&lanep->sync, ~sig);
}

/*
 * rpmem_fip_lane_signal -- signal lane about specified event and store
 * return value
 */
static inline void
rpmem_fip_lane_sigret(struct rpmem_fip_lane *lanep, uint64_t sig, int ret)
{
	lanep->ret = ret;
	rpmem_fip_lane_signal(lanep, sig);
}
