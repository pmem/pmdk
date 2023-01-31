/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * libpmemobj/types.h -- definitions of libpmemobj type-safe macros
 */
#ifndef LIBPMEMOBJ_TYPES_H
#define LIBPMEMOBJ_TYPES_H 1

#include <libpmemobj/base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOID_NULL(t)	((TOID(t))OID_NULL)
#define PMEMOBJ_MAX_LAYOUT ((size_t)1024)

/*
 * Type safety macros
 */
#if !defined __clang__

#define TOID_ASSIGN(o, value)(\
{\
	(o).oid = value;\
	(o); /* to avoid "error: statement with no effect" */\
})

#else /* __clang__ */

#define TOID_ASSIGN(o, value) ((o).oid = value, (o))

#endif

#define TOID_EQUALS(lhs, rhs)\
((lhs).oid.off == (rhs).oid.off &&\
	(lhs).oid.pool_uuid_lo == (rhs).oid.pool_uuid_lo)

/* type number of root object */
#define POBJ_ROOT_TYPE_NUM 0
#define _toid_struct
#define _toid_union
#define _toid_enum
#define _POBJ_LAYOUT_REF(name) (sizeof(_pobj_layout_##name##_ref))

/*
 * Typed OID
 */
#define TOID(t)\
union _toid_##t##_toid

#ifdef __cplusplus
#define _TOID_CONSTR(t)\
_toid_##t##_toid()\
{ }\
_toid_##t##_toid(PMEMoid _oid) : oid(_oid)\
{ }
#else
#define _TOID_CONSTR(t)
#endif

/*
 * Declaration of typed OID
 */
#define _TOID_DECLARE(t, i)\
typedef uint8_t _toid_##t##_toid_type_num[(i) + 1];\
TOID(t)\
{\
	_TOID_CONSTR(t)\
	PMEMoid oid;\
	t *_type;\
	_toid_##t##_toid_type_num *_type_num;\
}

/*
 * Declaration of typed OID of an object
 */
#define TOID_DECLARE(t, i) _TOID_DECLARE(t, i)

/*
 * Declaration of typed OID of a root object
 */
#define TOID_DECLARE_ROOT(t) _TOID_DECLARE(t, POBJ_ROOT_TYPE_NUM)

/*
 * Type number of specified type
 */
#define TOID_TYPE_NUM(t) (sizeof(_toid_##t##_toid_type_num) - 1)

/*
 * Type number of object read from typed OID
 */
#define TOID_TYPE_NUM_OF(o) (sizeof(*(o)._type_num) - 1)

/*
 * NULL check
 */
#define TOID_IS_NULL(o)	((o).oid.off == 0)

/*
 * Validates whether type number stored in typed OID is the same
 * as type number stored in object's metadata
 */
#define TOID_VALID(o) (TOID_TYPE_NUM_OF(o) == pmemobj_type_num((o).oid))

/*
 * Checks whether the object is of a given type
 */
#define OID_INSTANCEOF(o, t) (TOID_TYPE_NUM(t) == pmemobj_type_num(o))

/*
 * Begin of layout declaration
 */
#define POBJ_LAYOUT_BEGIN(name)\
typedef uint8_t _pobj_layout_##name##_ref[__COUNTER__ + 1]

/*
 * End of layout declaration
 */
#define POBJ_LAYOUT_END(name)\
typedef char _pobj_layout_##name##_cnt[__COUNTER__ + 1 -\
_POBJ_LAYOUT_REF(name)];

/*
 * Number of types declared inside layout without the root object
 */
#define POBJ_LAYOUT_TYPES_NUM(name) (sizeof(_pobj_layout_##name##_cnt) - 1)

/*
 * Declaration of typed OID inside layout declaration
 */
#define POBJ_LAYOUT_TOID(name, t)\
TOID_DECLARE(t, (__COUNTER__ + 1 - _POBJ_LAYOUT_REF(name)));

/*
 * Declaration of typed OID of root inside layout declaration
 */
#define POBJ_LAYOUT_ROOT(name, t)\
TOID_DECLARE_ROOT(t);

/*
 * Name of declared layout
 */
#define POBJ_LAYOUT_NAME(name) #name

#define TOID_TYPEOF(o) __typeof__(*(o)._type)

#define TOID_OFFSETOF(o, field) offsetof(TOID_TYPEOF(o), field)

#define DIRECT_RW(o) (\
{__typeof__(o) _o; _o._type = NULL; (void)_o;\
(__typeof__(*(o)._type) *)pmemobj_direct((o).oid); })
#define DIRECT_RO(o) ((const __typeof__(*(o)._type) *)pmemobj_direct((o).oid))

#define D_RW	DIRECT_RW
#define D_RO	DIRECT_RO

#ifdef __cplusplus
}
#endif
#endif	/* libpmemobj/types.h */
