// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * libpmemobj/pool.h -- definitions of libpmemobj pool macros
 */

#ifndef LIBPMEMOBJ_POOL_H
#define LIBPMEMOBJ_POOL_H 1

#include <libpmemobj/pool_base.h>
#include <libpmemobj/types.h>

#define POBJ_ROOT(pop, t) (\
(TOID(t))pmemobj_root((pop), sizeof(t)))

#endif	/* libpmemobj/pool.h */
