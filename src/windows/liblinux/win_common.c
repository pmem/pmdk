/*
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
 * win_common.c -- Our implementation of few missing POSIX APIs or LINUX
 * system calls in Windows
 */

/*
 * setenv -- change or add an environment variable
 */
int
setenv(const char *name, const char *value, int overwrite)
{
	errno_t err;

	/*
	 * If caller doesn't want to overwrite make sure that a environment
	 * variable with the same name doesn't exist.
	 */
	if (!overwrite && getenv(name))
		return 0;

	/*
	 * _putenv_s returns a non-zero error code on failure but setenv
	 * needs to return -1 on failure, let's translate the error code.
	 */
	if ((err = _putenv_s(name, value)) != 0) {
		errno = err;
		return -1;
	}

	return 0;
}

/*
 * unsetenv -- remove an environment variable
 */
int
unsetenv(const char *name)
{
	errno_t err;
	if ((err = _putenv_s(name, "")) != 0) {
		errno = err;
		return -1;
	}

	return 0;
}

/*
 * rand_r -- rand_r for windows
 *
 * XXX: RAND_MAX is equal 0x7fff on Windows, so to get 32 bit random number
 *	we need to merge two numbers returned by rand_s().
 *	It is not to the best solution as subsequences returned by rand_s are
 *	not guaranteed to be independent.
 *
 * XXX: Windows doesn't implement deterministic thread-safe pseudorandom
 *	generator (generator which can be initialized by seed ).
 *	We have to chose between a deterministic nonthread-safe generator
 *	(rand(), srand()) or a non-deterministic thread-safe generator(rand_s())
 *	as thread-safety is more important, a seed parameter is ignored in this
 *	implementation.
 */
int
rand_r(unsigned *seedp)
{
	UNREFERENCED_PARAMETER(seedp);
	unsigned part1, part2;
	rand_s(&part1);
	rand_s(&part2);
	return part1 << 16 | part2;
}
