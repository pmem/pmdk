/*
 * Copyright 2016, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * win_time.c -- Windows emulation of Linux-specific time functions
 */

#define NSEC_IN_SEC 1000000000ull
/* number of useconds between 1970-01-01T00:00:00Z and 1601-01-01T00:00:00Z */
#define DELTA_WIN2UNIX (11644473600000000ull)

/*
 * clock_gettime -- returns elapsed time since the system was restarted
 * or since Epoch, depending on the mode id
 */
int
clock_gettime(int id, struct timespec *ts)
{
	switch (id) {
	case CLOCK_MONOTONIC:
		{
			LARGE_INTEGER time;
			LARGE_INTEGER frequency;

			QueryPerformanceFrequency(&frequency);
			QueryPerformanceCounter(&time);

			ts->tv_sec = time.QuadPart / frequency.QuadPart;
			ts->tv_nsec = (long)(
				(time.QuadPart % frequency.QuadPart) *
				NSEC_IN_SEC / frequency.QuadPart);
		}
		break;

	case CLOCK_REALTIME:
		{
			FILETIME ctime_ft;
			GetSystemTimeAsFileTime(&ctime_ft);
			ULARGE_INTEGER ctime = {
				.HighPart = ctime_ft.dwHighDateTime,
				.LowPart = ctime_ft.dwLowDateTime,
			};
			ts->tv_sec = (ctime.QuadPart - DELTA_WIN2UNIX * 10)
				/ 10000000;
			ts->tv_nsec = ((ctime.QuadPart - DELTA_WIN2UNIX * 10)
				% 10000000) * 100;
		}
		break;

	default:
		SetLastError(EINVAL);
		return -1;
	}

	return 0;
}
