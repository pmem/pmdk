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
 * rpmem_timestamps.c -- unit test for rpmem timestamps
 */

#include "unittest.h"

#include "librpmem.h"
#include "out.h"

#include "rpmem.h"
#include "rpmem_timer.h"

#define LANE 1

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_timestamps");

	out_init(RPMEM_LOG_PREFIX, RPMEM_LOG_LEVEL_VAR, RPMEM_LOG_FILE_VAR,
			RPMEM_MAJOR_VERSION, RPMEM_MINOR_VERSION);
	rpmem_timer_init();

	RPMEM_TIME_MARK(RPMEM_TIMER_PERSIST_START, LANE);

	for (int event = 1; event < RPMEM_TIMER_N_EVENTS; event++) {
		RPMEM_TIME_START(event);
		RPMEM_TIME_STOP(event, LANE);
	}

	rpmem_timer_fini();
	out_fini();
	DONE(NULL);
}
