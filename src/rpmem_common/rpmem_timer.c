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
 * rpmem_timer.c -- definitions of timer for librpmem
 */

#ifdef RPMEM_TIMESTAMPS

#include <stdlib.h>
#include <sched.h>

#include "rpmem_common.h"
#include "rpmem_common_log.h"
#include "rpmem_timer.h"

#define NSECPSEC 1000000000

typedef unsigned eventindex_t;

struct timestamp {
	enum rpmem_timer_event event;
	unsigned lane;
	rpmem_timer_t difftime;
};

static eventindex_t eventindex;
static eventindex_t ntimestamps = 1024;
static struct timestamp *timestamps;

static const char *ts_str[RPMEM_TIMER_N_EVENTS] = {
	"RPMEM_TIMER_PERSIST_START",
	"RPMEM_TIMER_PERSIST",
	"RPMEM_TIMER_WAIT_EVENT",
	"RPMEM_TIMER_FI_WRITEMSG",
	"RPMEM_TIMER_FI_READMSG",
	"RPMEM_TIMER_FI_SENDMSG",
	"RPMEM_TIMER_FI_RECVMSG"
};

/*
 * rpmem_timer_init -- allocate an array for timestamps
 */
void
rpmem_timer_init(void)
{
	timestamps = calloc(ntimestamps, sizeof(struct timestamp));
	if (!timestamps) {
		RPMEMC_FATAL("allocating timestamps array failed");
	}
}

/*
 * rpmem_timer_get_nsecs -- (internal) get total number of nanoseconds
 */
static unsigned long long
rpmem_timer_get_nsecs(rpmem_timer_t *t)
{
	unsigned long long ret = (unsigned long long)t->tv_nsec;

	ret += (unsigned long long)(t->tv_sec * NSECPSEC);

	return ret;
}

/*
 * rpmem_timer_fini -- log all timestamps and free their array
 */
void
rpmem_timer_fini(void)
{
	eventindex_t i;

	if (eventindex > 0) {
		RPMEMC_LOG(NOTICE, "RPMEM_TIMESTAMP LOG BEGIN");
		for (i = 0; i < eventindex; i++) {
			RPMEMC_LOG(NOTICE, "RPMEM_TIMESTAMP(#%u): "
				"lane %u event %s time %llu ns",
				i, timestamps[i].lane,
				ts_str[timestamps[i].event],
				rpmem_timer_get_nsecs(&timestamps[i].difftime));
		}
		RPMEMC_LOG(NOTICE, "RPMEM_TIMESTAMP LOG END");
	}
	free(timestamps);
	timestamps = NULL;
}

/*
 * rpmem_timer_start -- get timestamp from clock source
 */
void
rpmem_timer_start(rpmem_timer_t *time)
{
	clock_gettime(CLOCK_MONOTONIC, time);
}

/*
 * rpmem_timer_diff -- (internal) get time interval
 */
static void
rpmem_timer_diff(rpmem_timer_t *d, rpmem_timer_t *t1,
		rpmem_timer_t *t2)
{
	long long nsecs = (t2->tv_sec  - t1->tv_sec) * NSECPSEC +
				t2->tv_nsec - t1->tv_nsec;
	RPMEM_ASSERT(nsecs >= 0);
	d->tv_sec = nsecs / NSECPSEC;
	d->tv_nsec = nsecs % NSECPSEC;
}

/* zeroed rpmem_timer_t structure */
static rpmem_timer_t zero_time;

/*
 * rpmem_timer_save -- save a timestamp
 */
void
rpmem_timer_save(rpmem_timer_t *difftime, enum rpmem_timer_event event,
			unsigned lane)
{
	eventindex_t index = __sync_fetch_and_add(&eventindex, 1);

	while (index > ntimestamps) {
		/* wait for another thread reallocating the array */
		sched_yield();
	}

	if (index == ntimestamps) {
		timestamps = realloc(timestamps,
				2 * ntimestamps * sizeof(struct timestamp));
		if (!timestamps) {
			RPMEMC_FATAL("reallocating timestamps array failed");
		}
		ntimestamps *= 2;
	}

	timestamps[index].event = event;
	timestamps[index].lane = lane;
	timestamps[index].difftime = (difftime) ? *difftime : zero_time;
}

/*
 * rpmem_timer_stop_save -- stop timer and save the timestamp
 */
void
rpmem_timer_stop_save(rpmem_timer_t *starttime, enum rpmem_timer_event event,
			unsigned lane)
{
	rpmem_timer_t stoptime, difftime;

	clock_gettime(CLOCK_MONOTONIC, &stoptime);
	rpmem_timer_diff(&difftime, starttime, &stoptime);
	rpmem_timer_save(&difftime, event, lane);
}

#endif
