// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * rand.c -- random utils
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "rand.h"

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
 * It has a period of 2**256 - 1, excluding all-zero state; it must always get
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
		if (syscall(SYS_getrandom, state, sizeof(rng_t), 0)
			== sizeof(rng_t)) {
			return; /* nofail, but ENOSYS on kernel < 3.16 */
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
