/*
 * Copyright 2019, Intel Corporation
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
 * rand.c -- random utils
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "rand.h"

#ifdef _WIN32
#include <bcrypt.h>
#include <process.h>
#else
#include <sys/syscall.h>
#endif

/*
 * hash64 -- a u64 -> u64 hash
 */
uint64_t
hash64(uint64_t x)
{
	x += 0x9e3779b97f4a7c15;
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
	x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
	return x ^ (x >> 31);
}

/*
 * xoshiro256** random generator
 *
 * Fastest available good PRNG as of 2018 (sub-nanosecond per entry), produces
 * much better output than old stuff like rand() or Mersenne's Twister.
 *
 * By David Blackman and Sebastiano Vigna; PD/CC0 2018.
 *
 * It has a period of 2²⁵⁶-1, excluding all-zero state; it must always get
 * initialized to avoid that zero.
 */

static inline uint64_t rotl(const uint64_t x, int k)
{
	/* optimized to a single instruction on x86 */
	return (x << k) | (x >> (64 - k));
}

/*
 * rnd64_r -- return 64-bits of randomness
 */
uint64_t
rnd64_r(rng_t *state)
{
	uint64_t *s = (void *)state;

	const uint64_t result = rotl(s[1] * 5, 7) * 9;
	const uint64_t t = s[1] << 17;

	s[2] ^= s[0];
	s[3] ^= s[1];
	s[1] ^= s[2];
	s[0] ^= s[3];

	s[2] ^= t;

	s[3] = rotl(s[3], 45);

	return result;
}

/*
 * randomize_r -- initialize random generator
 *
 * Seed of 0 means random.
 */
void
randomize_r(rng_t *state, uint64_t seed)
{
	if (!seed) {
#ifdef SYS_getrandom
		/* We want getentropy() but ancient Red Hat lacks it. */
		if (!syscall(SYS_getrandom, state, sizeof(rng_t), 0))
			return; /* nofail, but ENOSYS on kernel < 3.16 */
#elif _WIN32
#pragma comment(lib, "Bcrypt.lib")
		if (BCryptGenRandom(NULL, (PUCHAR)state, sizeof(rng_t),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG)) {
			return;
		}
#endif
		seed = (uint64_t)getpid();
	}

	uint64_t *s = (void *)state;
	s[0] = hash64(seed);
	s[1] = hash64(s[0]);
	s[2] = hash64(s[1]);
	s[3] = hash64(s[2]);
}

static rng_t global_rng;

/*
 * rnd64 -- global state version of rnd64_t
 */
uint64_t
rnd64(void)
{
	return rnd64_r(&global_rng);
}


/*
 * randomize -- initialize global RNG
 */
void
randomize(uint64_t seed)
{
	randomize_r(&global_rng, seed);
}
