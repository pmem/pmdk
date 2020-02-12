// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */
/*
 * pmemobj_list.h -- macro definitions for persistent
 * singly linked list and tail queue
 */

#ifndef PMEMOBJ_LISTS_H
#define PMEMOBJ_LISTS_H

#include <libpmemobj.h>

/*
 * This file defines two types of persistent data structures:
 * singly-linked lists and tail queue.
 *
 * All macros defined in this file must be used within libpmemobj
 * transactional API. Following snippet presents example of usage:
 *
 *	TX_BEGIN(pop) {
 *		POBJ_TAILQ_INIT(head);
 *	} TX_ONABORT {
 *		abort();
 *	} TX_END
 *
 *                              SLIST    TAILQ
 * _HEAD                        +        +
 * _ENTRY                       +        +
 * _INIT                        +        +
 * _EMPTY                       +        +
 * _FIRST                       +        +
 * _NEXT                        +        +
 * _PREV                        -        +
 * _LAST                        -        +
 * _FOREACH                     +        +
 * _FOREACH_REVERSE             -        +
 * _INSERT_HEAD                 +        +
 * _INSERT_BEFORE               -        +
 * _INSERT_AFTER                +        +
 * _INSERT_TAIL                 -        +
 * _MOVE_ELEMENT_HEAD           -        +
 * _MOVE_ELEMENT_TAIL           -        +
 * _REMOVE_HEAD                 +        -
 * _REMOVE                      +        +
 * _REMOVE_FREE                 +        +
 * _SWAP_HEAD_TAIL              -        +
 */

/*
 * Singly-linked List definitions.
 */
#define POBJ_SLIST_HEAD(name, type)\
struct name {\
	TOID(type) pe_first;\
}

#define POBJ_SLIST_ENTRY(type)\
struct {\
	TOID(type) pe_next;\
}

/*
 * Singly-linked List access methods.
 */
#define POBJ_SLIST_EMPTY(head)	(TOID_IS_NULL((head)->pe_first))
#define POBJ_SLIST_FIRST(head)	((head)->pe_first)
#define POBJ_SLIST_NEXT(elm, field)	(D_RO(elm)->field.pe_next)

/*
 * Singly-linked List functions.
 */
#define POBJ_SLIST_INIT(head) do {\
	TX_ADD_DIRECT(&(head)->pe_first);\
	TOID_ASSIGN((head)->pe_first, OID_NULL);\
} while (0)

#define POBJ_SLIST_INSERT_HEAD(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	TX_ADD_DIRECT(&elm_ptr->field.pe_next);\
	elm_ptr->field.pe_next = (head)->pe_first;\
	TX_SET_DIRECT(head, pe_first, elm);\
} while (0)

#define POBJ_SLIST_INSERT_AFTER(slistelm, elm, field) do {\
	TOID_TYPEOF(slistelm) *slistelm_ptr = D_RW(slistelm);\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	TX_ADD_DIRECT(&elm_ptr->field.pe_next);\
	elm_ptr->field.pe_next = slistelm_ptr->field.pe_next;\
	TX_ADD_DIRECT(&slistelm_ptr->field.pe_next);\
	slistelm_ptr->field.pe_next = elm;\
} while (0)

#define POBJ_SLIST_REMOVE_HEAD(head, field) do {\
	TX_ADD_DIRECT(&(head)->pe_first);\
	(head)->pe_first = D_RO((head)->pe_first)->field.pe_next;\
} while (0)

#define POBJ_SLIST_REMOVE(head, elm, field) do {\
	if (TOID_EQUALS((head)->pe_first, elm)) {\
		POBJ_SLIST_REMOVE_HEAD(head, field);\
	} else {\
		TOID_TYPEOF(elm) *curelm_ptr = D_RW((head)->pe_first);\
		while (!TOID_EQUALS(curelm_ptr->field.pe_next, elm))\
			curelm_ptr = D_RW(curelm_ptr->field.pe_next);\
		TX_ADD_DIRECT(&curelm_ptr->field.pe_next);\
		curelm_ptr->field.pe_next = D_RO(elm)->field.pe_next;\
	}\
} while (0)

#define POBJ_SLIST_REMOVE_FREE(head, elm, field) do {\
	POBJ_SLIST_REMOVE(head, elm, field);\
	TX_FREE(elm);\
} while (0)

#define POBJ_SLIST_FOREACH(var, head, field)\
	for ((var) = POBJ_SLIST_FIRST(head);\
		!TOID_IS_NULL(var);\
		var = POBJ_SLIST_NEXT(var, field))

/*
 * Tail-queue definitions.
 */
#define POBJ_TAILQ_ENTRY(type)\
struct {\
	TOID(type) pe_next;\
	TOID(type) pe_prev;\
}

#define POBJ_TAILQ_HEAD(name, type)\
struct name {\
	TOID(type) pe_first;\
	TOID(type) pe_last;\
}

/*
 * Tail-queue access methods.
 */
#define POBJ_TAILQ_FIRST(head)	((head)->pe_first)
#define POBJ_TAILQ_LAST(head)	((head)->pe_last)

#define POBJ_TAILQ_EMPTY(head)	(TOID_IS_NULL((head)->pe_first))
#define POBJ_TAILQ_NEXT(elm, field)	(D_RO(elm)->field.pe_next)
#define POBJ_TAILQ_PREV(elm, field)	(D_RO(elm)->field.pe_prev)

/*
 * Tail-queue List internal methods.
 */
#define _POBJ_SWAP_PTR(elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	TX_ADD_DIRECT(&elm_ptr->field);\
	__typeof__(elm) temp = elm_ptr->field.pe_prev;\
	elm_ptr->field.pe_prev = elm_ptr->field.pe_next;\
	elm_ptr->field.pe_next = temp;\
} while (0)

/*
 * Tail-queue functions.
 */
#define POBJ_TAILQ_SWAP_HEAD_TAIL(head, field) do {\
	__typeof__((head)->pe_first) temp = (head)->pe_first;\
	TX_ADD_DIRECT(head);\
	(head)->pe_first = (head)->pe_last;\
	(head)->pe_last = temp;\
} while (0)

#define POBJ_TAILQ_FOREACH(var, head, field)\
	for ((var) = POBJ_TAILQ_FIRST(head);\
		!TOID_IS_NULL(var);\
		var = POBJ_TAILQ_NEXT(var, field))

#define POBJ_TAILQ_FOREACH_REVERSE(var, head, field)\
	for ((var) = POBJ_TAILQ_LAST(head);\
		!TOID_IS_NULL(var);\
		var = POBJ_TAILQ_PREV(var, field))

#define POBJ_TAILQ_INIT(head) do {\
	TX_ADD_FIELD_DIRECT(head, pe_first);\
	TOID_ASSIGN((head)->pe_first, OID_NULL);\
	TX_ADD_FIELD_DIRECT(head, pe_last);\
	TOID_ASSIGN((head)->pe_last, OID_NULL);\
} while (0)

#define POBJ_TAILQ_INSERT_HEAD(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	if (TOID_IS_NULL((head)->pe_first)) {\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_prev = (head)->pe_first;\
		elm_ptr->field.pe_next = (head)->pe_first;\
		TX_ADD_DIRECT(head);\
		(head)->pe_first = elm;\
		(head)->pe_last = elm;\
	} else {\
		TOID_TYPEOF(elm) *first = D_RW((head)->pe_first);\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_next = (head)->pe_first;\
		elm_ptr->field.pe_prev = first->field.pe_prev;\
		TX_ADD_DIRECT(&first->field.pe_prev);\
		first->field.pe_prev = elm;\
		TX_SET_DIRECT(head, pe_first, elm);\
	}\
} while (0)

#define POBJ_TAILQ_INSERT_TAIL(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	if (TOID_IS_NULL((head)->pe_last)) {\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_prev = (head)->pe_last;\
		elm_ptr->field.pe_next = (head)->pe_last;\
		TX_ADD_DIRECT(head);\
		(head)->pe_first = elm;\
		(head)->pe_last = elm;\
	} else {\
		TOID_TYPEOF(elm) *last = D_RW((head)->pe_last);\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_prev = (head)->pe_last;\
		elm_ptr->field.pe_next = last->field.pe_next;\
		TX_ADD_DIRECT(&last->field.pe_next);\
		last->field.pe_next = elm;\
		TX_SET_DIRECT(head, pe_last, elm);\
	}\
} while (0)

#define POBJ_TAILQ_INSERT_AFTER(listelm, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	TOID_TYPEOF(listelm) *listelm_ptr = D_RW(listelm);\
	TX_ADD_DIRECT(&elm_ptr->field);\
	elm_ptr->field.pe_prev = listelm;\
	elm_ptr->field.pe_next = listelm_ptr->field.pe_next;\
	if (TOID_IS_NULL(listelm_ptr->field.pe_next)) {\
		TX_SET_DIRECT(head, pe_last, elm);\
	} else {\
		TOID_TYPEOF(elm) *next = D_RW(listelm_ptr->field.pe_next);\
		TX_ADD_DIRECT(&next->field.pe_prev);\
		next->field.pe_prev = elm;\
	}\
	TX_ADD_DIRECT(&listelm_ptr->field.pe_next);\
	listelm_ptr->field.pe_next = elm;\
} while (0)

#define POBJ_TAILQ_INSERT_BEFORE(listelm, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	TOID_TYPEOF(listelm) *listelm_ptr = D_RW(listelm);\
	TX_ADD_DIRECT(&elm_ptr->field);\
	elm_ptr->field.pe_next = listelm;\
	elm_ptr->field.pe_prev = listelm_ptr->field.pe_prev;\
	if (TOID_IS_NULL(listelm_ptr->field.pe_prev)) {\
		TX_SET_DIRECT(head, pe_first, elm);\
	} else {\
		TOID_TYPEOF(elm) *prev = D_RW(listelm_ptr->field.pe_prev);\
		TX_ADD_DIRECT(&prev->field.pe_next);\
		prev->field.pe_next = elm; \
	}\
	TX_ADD_DIRECT(&listelm_ptr->field.pe_prev);\
	listelm_ptr->field.pe_prev = elm;\
} while (0)

#define POBJ_TAILQ_REMOVE(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	if (TOID_IS_NULL(elm_ptr->field.pe_prev) &&\
		TOID_IS_NULL(elm_ptr->field.pe_next)) {\
		TX_ADD_DIRECT(head);\
		(head)->pe_first = elm_ptr->field.pe_prev;\
		(head)->pe_last = elm_ptr->field.pe_next;\
	} else {\
		if (TOID_IS_NULL(elm_ptr->field.pe_prev)) {\
			TX_SET_DIRECT(head, pe_first, elm_ptr->field.pe_next);\
			TOID_TYPEOF(elm) *next = D_RW(elm_ptr->field.pe_next);\
			TX_ADD_DIRECT(&next->field.pe_prev);\
			next->field.pe_prev = elm_ptr->field.pe_prev;\
		} else {\
			TOID_TYPEOF(elm) *prev = D_RW(elm_ptr->field.pe_prev);\
			TX_ADD_DIRECT(&prev->field.pe_next);\
			prev->field.pe_next = elm_ptr->field.pe_next;\
		}\
		if (TOID_IS_NULL(elm_ptr->field.pe_next)) {\
			TX_SET_DIRECT(head, pe_last, elm_ptr->field.pe_prev);\
			TOID_TYPEOF(elm) *prev = D_RW(elm_ptr->field.pe_prev);\
			TX_ADD_DIRECT(&prev->field.pe_next);\
			prev->field.pe_next = elm_ptr->field.pe_next;\
		} else {\
			TOID_TYPEOF(elm) *next = D_RW(elm_ptr->field.pe_next);\
			TX_ADD_DIRECT(&next->field.pe_prev);\
			next->field.pe_prev = elm_ptr->field.pe_prev;\
		}\
	}\
} while (0)

#define POBJ_TAILQ_REMOVE_FREE(head, elm, field) do {\
	POBJ_TAILQ_REMOVE(head, elm, field);\
	TX_FREE(elm);\
} while (0)

/*
 * 2 cases: only two elements, the rest possibilities
 * including that elm is the last one
 */
#define POBJ_TAILQ_MOVE_ELEMENT_HEAD(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	if (TOID_EQUALS((head)->pe_last, elm) &&\
		TOID_EQUALS(D_RO((head)->pe_first)->field.pe_next, elm)) {\
		_POBJ_SWAP_PTR(elm, field);\
		_POBJ_SWAP_PTR((head)->pe_first, field);\
		POBJ_TAILQ_SWAP_HEAD_TAIL(head, field);\
	} else {\
		TOID_TYPEOF(elm) *prev = D_RW(elm_ptr->field.pe_prev);\
		TX_ADD_DIRECT(&prev->field.pe_next);\
		prev->field.pe_next = elm_ptr->field.pe_next;\
		if (TOID_EQUALS((head)->pe_last, elm)) {\
			TX_SET_DIRECT(head, pe_last, elm_ptr->field.pe_prev);\
		} else {\
			TOID_TYPEOF(elm) *next = D_RW(elm_ptr->field.pe_next);\
			TX_ADD_DIRECT(&next->field.pe_prev);\
			next->field.pe_prev = elm_ptr->field.pe_prev;\
		}\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_prev = D_RO((head)->pe_first)->field.pe_prev;\
		elm_ptr->field.pe_next = (head)->pe_first;\
		TOID_TYPEOF(elm) *first = D_RW((head)->pe_first);\
		TX_ADD_DIRECT(&first->field.pe_prev);\
		first->field.pe_prev = elm;\
		TX_SET_DIRECT(head, pe_first, elm);\
	}\
} while (0)

#define POBJ_TAILQ_MOVE_ELEMENT_TAIL(head, elm, field) do {\
	TOID_TYPEOF(elm) *elm_ptr = D_RW(elm);\
	if (TOID_EQUALS((head)->pe_first, elm) &&\
		TOID_EQUALS(D_RO((head)->pe_last)->field.pe_prev, elm)) {\
		_POBJ_SWAP_PTR(elm, field);\
		_POBJ_SWAP_PTR((head)->pe_last, field);\
		POBJ_TAILQ_SWAP_HEAD_TAIL(head, field);\
	} else {\
		TOID_TYPEOF(elm) *next = D_RW(elm_ptr->field.pe_next);\
		TX_ADD_DIRECT(&next->field.pe_prev);\
		next->field.pe_prev = elm_ptr->field.pe_prev;\
		if (TOID_EQUALS((head)->pe_first, elm)) {\
			TX_SET_DIRECT(head, pe_first, elm_ptr->field.pe_next);\
		} else {	\
			TOID_TYPEOF(elm) *prev = D_RW(elm_ptr->field.pe_prev);\
			TX_ADD_DIRECT(&prev->field.pe_next);\
			prev->field.pe_next = elm_ptr->field.pe_next;\
		}\
		TX_ADD_DIRECT(&elm_ptr->field);\
		elm_ptr->field.pe_prev = (head)->pe_last;\
		elm_ptr->field.pe_next = D_RO((head)->pe_last)->field.pe_next;\
		__typeof__(elm_ptr) last = D_RW((head)->pe_last);\
		TX_ADD_DIRECT(&last->field.pe_next);\
		last->field.pe_next = elm;\
		TX_SET_DIRECT(head, pe_last, elm);\
	}	\
} while (0)

#endif /* PMEMOBJ_LISTS_H */
