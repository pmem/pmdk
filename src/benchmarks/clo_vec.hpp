/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */
/*
 * clo_vec.hpp -- command line options vector declarations
 */
#include "queue.h"
#include <cstdlib>

struct clo_vec_args {
	PMDK_TAILQ_ENTRY(clo_vec_args) next;
	void *args;
};

struct clo_vec_alloc {
	PMDK_TAILQ_ENTRY(clo_vec_alloc) next;
	void *ptr;
};

struct clo_vec_value {
	PMDK_TAILQ_ENTRY(clo_vec_value) next;
	void *ptr;
};

struct clo_vec_vlist {
	PMDK_TAILQ_HEAD(valueshead, clo_vec_value) head;
	size_t nvalues;
};

struct clo_vec {
	size_t size;
	PMDK_TAILQ_HEAD(argshead, clo_vec_args) args;
	size_t nargs;
	PMDK_TAILQ_HEAD(allochead, clo_vec_alloc) allocs;
	size_t nallocs;
};

struct clo_vec *clo_vec_alloc(size_t size);
void clo_vec_free(struct clo_vec *clovec);
void *clo_vec_get_args(struct clo_vec *clovec, size_t i);
int clo_vec_add_alloc(struct clo_vec *clovec, void *ptr);
int clo_vec_memcpy(struct clo_vec *clovec, size_t off, size_t size, void *ptr);
int clo_vec_memcpy_list(struct clo_vec *clovec, size_t off, size_t size,
			struct clo_vec_vlist *list);
struct clo_vec_vlist *clo_vec_vlist_alloc(void);
void clo_vec_vlist_free(struct clo_vec_vlist *list);
void clo_vec_vlist_add(struct clo_vec_vlist *list, void *ptr, size_t size);
