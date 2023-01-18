/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * libpmemobj/iterator_base.h -- definitions of libpmemobj iterator entry points
 */

#ifndef LIBPMEMOBJ_ITERATOR_BASE_H
#define LIBPMEMOBJ_ITERATOR_BASE_H 1

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The following functions allow access to the entire collection of objects.
 *
 * Use with conjunction with non-transactional allocations. Pmemobj pool acts
 * as a generic container (list) of objects that are not assigned to any
 * user-defined data structures.
 */

/*
 * Returns the first object of the specified type number.
 */
PMEMoid pmemobj_first(PMEMobjpool *pop);

/*
 * Returns the next object of the same type.
 */
PMEMoid pmemobj_next(PMEMoid oid);

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/iterator_base.h */
