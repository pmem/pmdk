/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

/*
 * libpmemobj/atomic.h -- definitions of libpmemobj atomic macros
 */

#ifndef LIBPMEMOBJ_ATOMIC_H
#define LIBPMEMOBJ_ATOMIC_H 1

#include <libpmemobj/atomic_base.h>
#include <libpmemobj/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POBJ_NEW(pop, o, t, constr, arg)\
pmemobj_alloc((pop), (PMEMoid *)(o), sizeof(t), TOID_TYPE_NUM(t),\
	(constr), (arg))

#define POBJ_ALLOC(pop, o, t, size, constr, arg)\
pmemobj_alloc((pop), (PMEMoid *)(o), (size), TOID_TYPE_NUM(t),\
	(constr), (arg))

#define POBJ_ZNEW(pop, o, t)\
pmemobj_zalloc((pop), (PMEMoid *)(o), sizeof(t), TOID_TYPE_NUM(t))

#define POBJ_ZALLOC(pop, o, t, size)\
pmemobj_zalloc((pop), (PMEMoid *)(o), (size), TOID_TYPE_NUM(t))

#define POBJ_REALLOC(pop, o, t, size)\
pmemobj_realloc((pop), (PMEMoid *)(o), (size), TOID_TYPE_NUM(t))

#define POBJ_ZREALLOC(pop, o, t, size)\
pmemobj_zrealloc((pop), (PMEMoid *)(o), (size), TOID_TYPE_NUM(t))

#define POBJ_FREE(o)\
pmemobj_free((PMEMoid *)(o))

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/atomic.h */
