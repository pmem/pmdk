// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * libpmemobj/tx.h -- definitions of libpmemobj transactional macros
 */

#ifndef LIBPMEMOBJ_TX_H
#define LIBPMEMOBJ_TX_H 1

#include <errno.h>
#include <string.h>

#include <libpmemobj/tx_base.h>
#include <libpmemobj/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef POBJ_TX_CRASH_ON_NO_ONABORT
#define TX_ONABORT_CHECK do {\
		if (_stage == TX_STAGE_ONABORT)\
			abort();\
	} while (0)
#else
#define TX_ONABORT_CHECK do {} while (0)
#endif

#define _POBJ_TX_BEGIN(pop, ...)\
{\
	jmp_buf _tx_env;\
	enum pobj_tx_stage _stage;\
	int _pobj_errno;\
	if (setjmp(_tx_env)) {\
		errno = pmemobj_tx_errno();\
	} else {\
		_pobj_errno = pmemobj_tx_begin(pop, _tx_env, __VA_ARGS__,\
				TX_PARAM_NONE);\
		if (_pobj_errno)\
			errno = _pobj_errno;\
	}\
	while ((_stage = pmemobj_tx_stage()) != TX_STAGE_NONE) {\
		switch (_stage) {\
			case TX_STAGE_WORK:

#define TX_BEGIN_PARAM(pop, ...)\
_POBJ_TX_BEGIN(pop, ##__VA_ARGS__)

#define TX_BEGIN_LOCK TX_BEGIN_PARAM

/* Just to let compiler warn when incompatible function pointer is used */
static inline pmemobj_tx_callback
_pobj_validate_cb_sig(pmemobj_tx_callback cb)
{
	return cb;
}

#define TX_BEGIN_CB(pop, cb, arg, ...) _POBJ_TX_BEGIN(pop, TX_PARAM_CB,\
		_pobj_validate_cb_sig(cb), arg, ##__VA_ARGS__)

#define TX_BEGIN(pop) _POBJ_TX_BEGIN(pop, TX_PARAM_NONE)

#define TX_ONABORT\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_ONABORT:

#define TX_ONCOMMIT\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_ONCOMMIT:

#define TX_FINALLY\
				pmemobj_tx_process();\
				break;\
			case TX_STAGE_FINALLY:

#define TX_END\
				pmemobj_tx_process();\
				break;\
			default:\
				TX_ONABORT_CHECK;\
				pmemobj_tx_process();\
				break;\
		}\
	}\
	_pobj_errno = pmemobj_tx_end();\
	if (_pobj_errno)\
		errno = _pobj_errno;\
}

#define TX_ADD(o)\
pmemobj_tx_add_range((o).oid, 0, sizeof(*(o)._type))

#define TX_ADD_FIELD(o, field)\
	TX_ADD_DIRECT(&(D_RO(o)->field))

#define TX_ADD_DIRECT(p)\
pmemobj_tx_add_range_direct(p, sizeof(*(p)))

#define TX_ADD_FIELD_DIRECT(p, field)\
pmemobj_tx_add_range_direct(&(p)->field, sizeof((p)->field))

#define TX_XADD(o, flags)\
pmemobj_tx_xadd_range((o).oid, 0, sizeof(*(o)._type), flags)

#define TX_XADD_FIELD(o, field, flags)\
	TX_XADD_DIRECT(&(D_RO(o)->field), flags)

#define TX_XADD_DIRECT(p, flags)\
pmemobj_tx_xadd_range_direct(p, sizeof(*(p)), flags)

#define TX_XADD_FIELD_DIRECT(p, field, flags)\
pmemobj_tx_xadd_range_direct(&(p)->field, sizeof((p)->field), flags)

#define TX_NEW(t)\
((TOID(t))pmemobj_tx_alloc(sizeof(t), TOID_TYPE_NUM(t)))

#define TX_ALLOC(t, size)\
((TOID(t))pmemobj_tx_alloc(size, TOID_TYPE_NUM(t)))

#define TX_ZNEW(t)\
((TOID(t))pmemobj_tx_zalloc(sizeof(t), TOID_TYPE_NUM(t)))

#define TX_ZALLOC(t, size)\
((TOID(t))pmemobj_tx_zalloc(size, TOID_TYPE_NUM(t)))

#define TX_XALLOC(t, size, flags)\
((TOID(t))pmemobj_tx_xalloc(size, TOID_TYPE_NUM(t), flags))

/* XXX - not available when compiled with VC++ as C code (/TC) */
#if !defined(_MSC_VER) || defined(__cplusplus)
#define TX_REALLOC(o, size)\
((__typeof__(o))pmemobj_tx_realloc((o).oid, size, TOID_TYPE_NUM_OF(o)))

#define TX_ZREALLOC(o, size)\
((__typeof__(o))pmemobj_tx_zrealloc((o).oid, size, TOID_TYPE_NUM_OF(o)))
#endif /* !defined(_MSC_VER) || defined(__cplusplus) */

#define TX_STRDUP(s, type_num)\
pmemobj_tx_strdup(s, type_num)

#define TX_XSTRDUP(s, type_num, flags)\
pmemobj_tx_xstrdup(s, type_num, flags)

#define TX_WCSDUP(s, type_num)\
pmemobj_tx_wcsdup(s, type_num)

#define TX_XWCSDUP(s, type_num, flags)\
pmemobj_tx_xwcsdup(s, type_num, flags)

#define TX_FREE(o)\
pmemobj_tx_free((o).oid)

#define TX_XFREE(o, flags)\
pmemobj_tx_xfree((o).oid, flags)

#define TX_SET(o, field, value) (\
	TX_ADD_FIELD(o, field),\
	D_RW(o)->field = (value))

#define TX_SET_DIRECT(p, field, value) (\
	TX_ADD_FIELD_DIRECT(p, field),\
	(p)->field = (value))

static inline void *
TX_MEMCPY(void *dest, const void *src, size_t num)
{
	pmemobj_tx_add_range_direct(dest, num);
	return memcpy(dest, src, num);
}

static inline void *
TX_MEMSET(void *dest, int c, size_t num)
{
	pmemobj_tx_add_range_direct(dest, num);
	return memset(dest, c, num);
}

#ifdef __cplusplus
}
#endif

#endif	/* libpmemobj/tx.h */
