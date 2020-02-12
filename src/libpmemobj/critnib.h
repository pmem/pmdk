// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * critnib.h -- internal definitions for critnib tree
 */

#ifndef LIBPMEMOBJ_CRITNIB_H
#define LIBPMEMOBJ_CRITNIB_H 1

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct critnib;

struct critnib *critnib_new(void);
void critnib_delete(struct critnib *c);

int critnib_insert(struct critnib *c, uint64_t key, void *value);
void *critnib_remove(struct critnib *c, uint64_t key);
void *critnib_get(struct critnib *c, uint64_t key);
void *critnib_find_le(struct critnib *c, uint64_t key);

#ifdef __cplusplus
}
#endif

#endif
