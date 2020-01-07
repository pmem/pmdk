// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * libpmemobj/lists_atomic.h -- definitions of libpmemobj atomic lists macros
 */

#ifndef LIBPMEMOBJ_LISTS_ATOMIC_H
#define LIBPMEMOBJ_LISTS_ATOMIC_H 1

#include <libpmemobj/lists_atomic_base.h>
#include <libpmemobj/thread.h>
#include <libpmemobj/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Non-transactional persistent atomic circular doubly-linked list
 */
#define POBJ_LIST_ENTRY(type)\
struct {\
	TOID(type) pe_next;\
	TOID(type) pe_prev;\
}

#define POBJ_LIST_HEAD(name, type)\
struct name {\
	TOID(type) pe_first;\
	PMEMmutex lock;\
}

#define POBJ_LIST_FIRST(head)	((head)->pe_first)
#define POBJ_LIST_LAST(head, field) (\
TOID_IS_NULL((head)->pe_first) ?\
(head)->pe_first :\
D_RO((head)->pe_first)->field.pe_prev)

#define POBJ_LIST_EMPTY(head)	(TOID_IS_NULL((head)->pe_first))
#define POBJ_LIST_NEXT(elm, field)	(D_RO(elm)->field.pe_next)
#define POBJ_LIST_PREV(elm, field)	(D_RO(elm)->field.pe_prev)
#define POBJ_LIST_DEST_HEAD	1
#define POBJ_LIST_DEST_TAIL	0
#define POBJ_LIST_DEST_BEFORE	1
#define POBJ_LIST_DEST_AFTER	0

#define POBJ_LIST_FOREACH(var, head, field)\
for (_pobj_debug_notice("POBJ_LIST_FOREACH", __FILE__, __LINE__),\
	(var) = POBJ_LIST_FIRST((head));\
	TOID_IS_NULL((var)) == 0;\
	TOID_EQUALS(POBJ_LIST_NEXT((var), field),\
	POBJ_LIST_FIRST((head))) ?\
	TOID_ASSIGN((var), OID_NULL) :\
	((var) = POBJ_LIST_NEXT((var), field)))

#define POBJ_LIST_FOREACH_REVERSE(var, head, field)\
for (_pobj_debug_notice("POBJ_LIST_FOREACH_REVERSE", __FILE__, __LINE__),\
	(var) = POBJ_LIST_LAST((head), field);\
	TOID_IS_NULL((var)) == 0;\
	TOID_EQUALS(POBJ_LIST_PREV((var), field),\
	POBJ_LIST_LAST((head), field)) ?\
	TOID_ASSIGN((var), OID_NULL) :\
	((var) = POBJ_LIST_PREV((var), field)))

#define POBJ_LIST_INSERT_HEAD(pop, head, elm, field)\
pmemobj_list_insert((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), OID_NULL,\
	POBJ_LIST_DEST_HEAD, (elm).oid)

#define POBJ_LIST_INSERT_TAIL(pop, head, elm, field)\
pmemobj_list_insert((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), OID_NULL,\
	POBJ_LIST_DEST_TAIL, (elm).oid)

#define POBJ_LIST_INSERT_AFTER(pop, head, listelm, elm, field)\
pmemobj_list_insert((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), (listelm).oid,\
	0 /* after */, (elm).oid)

#define POBJ_LIST_INSERT_BEFORE(pop, head, listelm, elm, field)\
pmemobj_list_insert((pop), \
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), (listelm).oid,\
	1 /* before */, (elm).oid)

#define POBJ_LIST_INSERT_NEW_HEAD(pop, head, field, size, constr, arg)\
pmemobj_list_insert_new((pop),\
	TOID_OFFSETOF((head)->pe_first, field),\
	(head), OID_NULL, POBJ_LIST_DEST_HEAD, (size),\
	TOID_TYPE_NUM_OF((head)->pe_first), (constr), (arg))

#define POBJ_LIST_INSERT_NEW_TAIL(pop, head, field, size, constr, arg)\
pmemobj_list_insert_new((pop),\
	TOID_OFFSETOF((head)->pe_first, field),\
	(head), OID_NULL, POBJ_LIST_DEST_TAIL, (size),\
	TOID_TYPE_NUM_OF((head)->pe_first), (constr), (arg))

#define POBJ_LIST_INSERT_NEW_AFTER(pop, head, listelm, field, size,\
	constr, arg)\
pmemobj_list_insert_new((pop),\
	TOID_OFFSETOF((head)->pe_first, field),\
	(head), (listelm).oid, 0 /* after */, (size),\
	TOID_TYPE_NUM_OF((head)->pe_first), (constr), (arg))

#define POBJ_LIST_INSERT_NEW_BEFORE(pop, head, listelm, field, size,\
		constr, arg)\
pmemobj_list_insert_new((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), (listelm).oid, 1 /* before */, (size),\
	TOID_TYPE_NUM_OF((head)->pe_first), (constr), (arg))

#define POBJ_LIST_REMOVE(pop, head, elm, field)\
pmemobj_list_remove((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), (elm).oid, 0 /* no free */)

#define POBJ_LIST_REMOVE_FREE(pop, head, elm, field)\
pmemobj_list_remove((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head), (elm).oid, 1 /* free */)

#define POBJ_LIST_MOVE_ELEMENT_HEAD(pop, head, head_new, elm, field, field_new)\
pmemobj_list_move((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head_new), field_new),\
	(head_new), OID_NULL, POBJ_LIST_DEST_HEAD, (elm).oid)

#define POBJ_LIST_MOVE_ELEMENT_TAIL(pop, head, head_new, elm, field, field_new)\
pmemobj_list_move((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head_new), field_new),\
	(head_new), OID_NULL, POBJ_LIST_DEST_TAIL, (elm).oid)

#define POBJ_LIST_MOVE_ELEMENT_AFTER(pop,\
	head, head_new, listelm, elm, field, field_new)\
pmemobj_list_move((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head_new), field_new),\
	(head_new),\
	(listelm).oid,\
	0 /* after */, (elm).oid)

#define POBJ_LIST_MOVE_ELEMENT_BEFORE(pop,\
	head, head_new, listelm, elm, field, field_new)\
pmemobj_list_move((pop),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head), field),\
	(head),\
	TOID_OFFSETOF(POBJ_LIST_FIRST(head_new), field_new),\
	(head_new),\
	(listelm).oid,\
	1 /* before */, (elm).oid)

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/lists_atomic.h */
