// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * rand.h -- random utils
 */

#ifndef RAND_H
#define RAND_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t rng_t[4];

uint64_t hash64(uint64_t x);
uint64_t rnd64_r(rng_t *rng);
void randomize_r(rng_t *rng, uint64_t seed);
uint64_t rnd64(void);
void randomize(uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif
