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
 * win_time.c -- Windows emulation of Linux-specific time functions
 */

#define MILISEC_IN_SEC 1000
#define NANOSEC_IN_MILISEC 1000000

/*
 * clock_gettime -- returns elapsed time since the system was restarted
 * or since Epoch, depending on the mode id
 */
int
clock_gettime(int id, struct timespec *ts)
{
	switch (id) {
	case CLOCK_MONOTONIC:;
		unsigned long long elapsed_time_milisec;
		unsigned long milisecond;

		/*
		 * GetTickCount retrieves the number of milliseconds
		 * that have elapsed since the system was started
		 */
		elapsed_time_milisec = GetTickCount64();
		if (elapsed_time_milisec < MILISEC_IN_SEC) {
			ts->tv_sec = elapsed_time_milisec / MILISEC_IN_SEC;
			ts->tv_nsec = (unsigned long)elapsed_time_milisec
				* NANOSEC_IN_MILISEC;
		} else {
			milisecond = elapsed_time_milisec %  MILISEC_IN_SEC;
			ts->tv_sec = (elapsed_time_milisec - milisecond)
				/ MILISEC_IN_SEC;
			ts->tv_nsec = milisecond *  NANOSEC_IN_MILISEC;
		}
		break;

	default:
		SetLastError(EINVAL);
		return -1;
	}

	return 0;
}
