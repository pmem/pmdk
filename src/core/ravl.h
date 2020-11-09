/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */

/*
 * ravl.h -- internal definitions for ravl tree
 */

#ifndef LIBPMEMOBJ_RAVL_H
#define LIBPMEMOBJ_RAVL_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ravl;
struct ravl_node;

enum ravl_predicate {
	RAVL_PREDICATE_EQUAL		=	1 << 0,
	RAVL_PREDICATE_GREATER		=	1 << 1,
	RAVL_PREDICATE_LESS		=	1 << 2,
	RAVL_PREDICATE_LESS_EQUAL	=
		RAVL_PREDICATE_EQUAL | RAVL_PREDICATE_LESS,
	RAVL_PREDICATE_GREATER_EQUAL	=
		RAVL_PREDICATE_EQUAL | RAVL_PREDICATE_GREATER,
};

typedef int ravl_compare(const void *lhs, const void *rhs);
typedef void ravl_cb(void *data, void *arg);
typedef void ravl_constr(void *data, size_t data_size, const void *arg);

struct ravl *ravl_new(ravl_compare *compare);
struct ravl *ravl_new_sized(ravl_compare *compare, size_t data_size);
void ravl_delete(struct ravl *ravl);
void ravl_delete_cb(struct ravl *ravl, ravl_cb cb, void *arg);
void ravl_foreach(struct ravl *ravl, ravl_cb cb, void *arg);
int ravl_empty(struct ravl *ravl);
void ravl_clear(struct ravl *ravl);
int ravl_insert(struct ravl *ravl, const void *data);
int ravl_emplace(struct ravl *ravl, ravl_constr constr, const void *arg);
int ravl_emplace_copy(struct ravl *ravl, const void *data);

struct ravl_node *ravl_find(struct ravl *ravl, const void *data,
	enum ravl_predicate predicate_flags);
struct ravl_node *ravl_first(struct ravl *ravl);
void *ravl_data(struct ravl_node *node);
void ravl_remove(struct ravl *ravl, struct ravl_node *node);

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_RAVL_H */
