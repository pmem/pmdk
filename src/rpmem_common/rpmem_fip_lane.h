/*
 * Copyright 2016-2017, Intel Corporation
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
#include "sys_util.h"
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
	os_spinlock_t lock;
	int ret;
	uint64_t sync;
};

/*
 * rpmem_fip_lane_init -- initialize basic lane structure
 */
static inline int
rpmem_fip_lane_init(struct rpmem_fip_lane *lanep)
{
	lanep->ret = 0;
	lanep->sync = 0;

	return util_spin_init(&lanep->lock, PTHREAD_PROCESS_PRIVATE);
}

/*
 * rpmem_fip_lane_fini -- deinitialize basic lane structure
 */
static inline void
rpmem_fip_lane_fini(struct rpmem_fip_lane *lanep)
{
	util_spin_destroy(&lanep->lock);
}

/*
 * rpmem_fip_lane_busy -- return true if lane has pending events
 */
static inline int
rpmem_fip_lane_busy(struct rpmem_fip_lane *lanep)
{
	util_spin_lock(&lanep->lock);
	int ret = lanep->sync != 0;
	util_spin_unlock(&lanep->lock);

	return ret;
}

/*
 * rpmem_fip_lane_begin -- begin waiting for specified event(s)
 */
static inline void
rpmem_fip_lane_begin(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	util_spin_lock(&lanep->lock);
	lanep->ret = 0;
	lanep->sync |= sig;
	util_spin_unlock(&lanep->lock);
}

static inline int
rpmem_fip_lane_is_busy(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	util_spin_lock(&lanep->lock);
	int ret = (lanep->sync & sig) != 0;
	util_spin_unlock(&lanep->lock);

	return ret;
}

static inline int
rpmem_fip_lane_ret(struct rpmem_fip_lane *lanep)
{
	util_spin_lock(&lanep->lock);
	int ret = lanep->ret;
	util_spin_unlock(&lanep->lock);

	return ret;
}

/*
 * rpmem_fip_lane_wait -- wait for specified event(s)
 */
static inline int
rpmem_fip_lane_wait(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	while (rpmem_fip_lane_is_busy(lanep, sig))
		sched_yield();

	return rpmem_fip_lane_ret(lanep);
}

/*
 * rpmem_fip_lane_signal -- signal lane about specified event
 */
static inline void
rpmem_fip_lane_signal(struct rpmem_fip_lane *lanep, uint64_t sig)
{
	util_spin_lock(&lanep->lock);
	lanep->sync &= ~sig;
	util_spin_unlock(&lanep->lock);
}

/*
 * rpmem_fip_lane_signal -- signal lane about specified event and store
 * return value
 */
static inline void
rpmem_fip_lane_sigret(struct rpmem_fip_lane *lanep, uint64_t sig, int ret)
{
	util_spin_lock(&lanep->lock);
	lanep->ret = ret;
	lanep->sync &= ~sig;
	util_spin_unlock(&lanep->lock);
}
