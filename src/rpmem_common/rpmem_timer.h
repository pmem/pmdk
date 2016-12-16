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
 * rpmem_timer.h -- definitions of timer for librpmem
 */

#ifdef RPMEM_TIMESTAMPS

#ifndef RPMEMC_LOG_RPMEM
#error The RPMEMC_LOG_RPMEM macro must be defined to use RPMEM timestamps
#endif

#include <time.h>

typedef struct timespec rpmem_timer_t;

/*
 * rpmem_timer_event  -- events of rpmem timer
 */
enum rpmem_timer_event {
	RPMEM_TIMER_PERSIST_START,	/* beginning of persist operation */
	RPMEM_TIMER_PERSIST,		/* time of persist operation */
	RPMEM_TIMER_WAIT_EVENT,		/* time of waiting for an event */
	RPMEM_TIMER_FI_WRITEMSG,	/* time of fi_writemsg operation */
	RPMEM_TIMER_FI_READMSG,		/* time of fi_readmsg operation */
	RPMEM_TIMER_FI_SENDMSG,		/* time of fi_sendmsg operation */
	RPMEM_TIMER_FI_RECVMSG,		/* time of fi_recvmsg operation */
	RPMEM_TIMER_N_EVENTS		/* number of timer events */
};

void rpmem_timer_init(void);
void rpmem_timer_fini(void);
void rpmem_timer_start(rpmem_timer_t *time);
void rpmem_timer_save(rpmem_timer_t *difftime,
				enum rpmem_timer_event event, unsigned lane);
void rpmem_timer_stop_save(rpmem_timer_t *starttime,
				enum rpmem_timer_event event, unsigned lane);

/*
 * RPMEM_TIME_START -- define and start the timer for the 'event'
 */
#define RPMEM_TIME_START(event)\
	rpmem_timer_t timer##event;\
	rpmem_timer_start(&timer##event)

/*
 * RPMEM_TIME_STOP -- stop and save the timer for the 'event'
 */
#define RPMEM_TIME_STOP(event, lane)\
	rpmem_timer_stop_save(&timer##event, event, lane)

/*
 * RPMEM_TIME_MARK -- save the event mark for the 'event'
 */
#define RPMEM_TIME_MARK(event, lane)\
	rpmem_timer_save(NULL, event, lane)

#else

#define RPMEM_TIME_START(event)\
	do { } while (0)

#define RPMEM_TIME_STOP(event, lane)\
	do { } while (0)

#define RPMEM_TIME_MARK(event, lane)\
	do { } while (0)

#endif
