/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

#ifndef HASHSET_INTERNAL_H
#define HASHSET_INTERNAL_H

/* large prime number used as a hashing function coefficient */
#define HASH_FUNC_COEFF_P 32212254719ULL

/* initial number of buckets */
#define INIT_BUCKETS_NUM 10

/* number of values in a bucket which trigger hashtable rebuild check */
#define MIN_HASHSET_THRESHOLD 5

/* number of values in a bucket which force hashtable rebuild */
#define MAX_HASHSET_THRESHOLD 10

#endif
