/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

/*
 * libpmemobj/action.h -- definitions of libpmemobj action interface
 */

#ifndef LIBPMEMOBJ_ACTION_H
#define LIBPMEMOBJ_ACTION_H 1

#include <libpmemobj/action_base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POBJ_RESERVE_NEW(pop, t, act)\
((TOID(t))pmemobj_reserve(pop, act, sizeof(t), TOID_TYPE_NUM(t)))

#define POBJ_RESERVE_ALLOC(pop, t, size, act)\
((TOID(t))pmemobj_reserve(pop, act, size, TOID_TYPE_NUM(t)))

#define POBJ_XRESERVE_NEW(pop, t, act, flags)\
((TOID(t))pmemobj_xreserve(pop, act, sizeof(t), TOID_TYPE_NUM(t), flags))

#define POBJ_XRESERVE_ALLOC(pop, t, size, act, flags)\
((TOID(t))pmemobj_xreserve(pop, act, size, TOID_TYPE_NUM(t), flags))

#ifdef __cplusplus
}
#endif

#endif /* libpmemobj/action_base.h */
