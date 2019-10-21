/*
 * Copyright 2014-2019, Intel Corporation
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
 * libpmemobj/iterator.h -- definitions of libpmemobj iterator macros
 */

#ifndef LIBPMEMOBJ_ITERATOR_H
#define LIBPMEMOBJ_ITERATOR_H 1

#include <libpmemobj/iterator_base.h>
#include <libpmemobj/types.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline PMEMoid
POBJ_FIRST_TYPE_NUM(PMEMobjpool *pop, uint64_t type_num)
{
	PMEMoid _pobj_ret = pmemobj_first(pop);

	while (!OID_IS_NULL(_pobj_ret) &&
			pmemobj_type_num(_pobj_ret) != type_num) {
		_pobj_ret = pmemobj_next(_pobj_ret);
	}
	return _pobj_ret;
}

static inline PMEMoid
POBJ_NEXT_TYPE_NUM(PMEMoid o)
{
	PMEMoid _pobj_ret = o;

	do {
		_pobj_ret = pmemobj_next(_pobj_ret);\
	} while (!OID_IS_NULL(_pobj_ret) &&
			pmemobj_type_num(_pobj_ret) != pmemobj_type_num(o));
	return _pobj_ret;
}

#define POBJ_FIRST(pop, t) ((TOID(t))POBJ_FIRST_TYPE_NUM(pop, TOID_TYPE_NUM(t)))

#define POBJ_NEXT(o) ((__typeof__(o))POBJ_NEXT_TYPE_NUM((o).oid))

/*
 * Iterates through every existing allocated object.
 */
#define POBJ_FOREACH(pop, varoid)\
for (_pobj_debug_notice("POBJ_FOREACH", __FILE__, __LINE__),\
	varoid = pmemobj_first(pop);\
		(varoid).off != 0; varoid = pmemobj_next(varoid))

/*
 * Safe variant of POBJ_FOREACH in which pmemobj_free on varoid is allowed
 */
#define POBJ_FOREACH_SAFE(pop, varoid, nvaroid)\
for (_pobj_debug_notice("POBJ_FOREACH_SAFE", __FILE__, __LINE__),\
	varoid = pmemobj_first(pop);\
		(varoid).off != 0 && (nvaroid = pmemobj_next(varoid), 1);\
		varoid = nvaroid)

/*
 * Iterates through every object of the specified type.
 */
#define POBJ_FOREACH_TYPE(pop, var)\
POBJ_FOREACH(pop, (var).oid)\
if (pmemobj_type_num((var).oid) == TOID_TYPE_NUM_OF(var))

/*
 * Safe variant of POBJ_FOREACH_TYPE in which pmemobj_free on var
 * is allowed.
 */
#define POBJ_FOREACH_SAFE_TYPE(pop, var, nvar)\
POBJ_FOREACH_SAFE(pop, (var).oid, (nvar).oid)\
if (pmemobj_type_num((var).oid) == TOID_TYPE_NUM_OF(var))

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/iterator.h */
