/*
 * Copyright 2015-2017, Intel Corporation
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
 * ctree.h -- internal definitions for crit-bit tree
 */

#ifndef LIBPMEMOBJ_CTREE_H
#define LIBPMEMOBJ_CTREE_H 1

#include <stdint.h>

struct ctree;

struct ctree *ctree_new(void);
void ctree_delete(struct ctree *t);
typedef void (*ctree_destroy_cb)(uint64_t key, uint64_t value, void *ctx);
void ctree_delete_cb(struct ctree *t, ctree_destroy_cb cb, void *ctx);

void ctree_clear(struct ctree *t);
void ctree_clear_unlocked(struct ctree *t);

int ctree_insert(struct ctree *t, uint64_t key, uint64_t value);
int ctree_insert_unlocked(struct ctree *t, uint64_t key, uint64_t value);

uint64_t ctree_find(struct ctree *t, uint64_t key);
uint64_t ctree_find_unlocked(struct ctree *t, uint64_t key);

uint64_t ctree_find_le(struct ctree *t, uint64_t *key);
uint64_t ctree_find_le_unlocked(struct ctree *t, uint64_t *key);

uint64_t ctree_remove(struct ctree *t, uint64_t key, int eq);
uint64_t ctree_remove_unlocked(struct ctree *t, uint64_t key, int eq);

int ctree_remove_max(struct ctree *t, uint64_t *key, uint64_t *value);
int ctree_remove_max_unlocked(struct ctree *t, uint64_t *key, uint64_t *value);

int ctree_is_empty(struct ctree *t);
int ctree_is_empty_unlocked(struct ctree *t);

#endif
