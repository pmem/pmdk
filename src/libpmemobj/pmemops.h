/*
 * Copyright 2016-2018, Intel Corporation
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

#ifndef LIBPMEMOBJ_PMEMOPS_H
#define LIBPMEMOBJ_PMEMOPS_H 1

#include <stddef.h>
#include <stdint.h>
#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*persist_fn)(void *base, const void *, size_t, unsigned);
typedef int (*flush_fn)(void *base, const void *, size_t, unsigned);
typedef void (*drain_fn)(void *base);

typedef void *(*memcpy_fn)(void *base, void *dest, const void *src, size_t len,
		unsigned flags);
typedef void *(*memmove_fn)(void *base, void *dest, const void *src, size_t len,
		unsigned flags);
typedef void *(*memset_fn)(void *base, void *dest, int c, size_t len,
		unsigned flags);

typedef int (*remote_read_fn)(void *ctx, uintptr_t base, void *dest, void *addr,
		size_t length);

struct pmem_ops {
	/* for 'master' replica: with or without data replication */
	persist_fn persist;	/* persist function */
	flush_fn flush;		/* flush function */
	drain_fn drain;		/* drain function */
	memcpy_fn memcpy; /* persistent memcpy function */
	memmove_fn memmove; /* persistent memmove function */
	memset_fn memset; /* persistent memset function */
	void *base;

	struct remote_ops {
		remote_read_fn read;

		void *ctx;
		uintptr_t base;
	} remote;
};

static force_inline int
pmemops_xpersist(const struct pmem_ops *p_ops, const void *d, size_t s,
		unsigned flags)
{
	return p_ops->persist(p_ops->base, d, s, flags);
}

static force_inline void
pmemops_persist(const struct pmem_ops *p_ops, const void *d, size_t s)
{
	(void) pmemops_xpersist(p_ops, d, s, 0);
}

static force_inline int
pmemops_xflush(const struct pmem_ops *p_ops, const void *d, size_t s,
		unsigned flags)
{
	return p_ops->flush(p_ops->base, d, s, flags);
}

static force_inline void
pmemops_flush(const struct pmem_ops *p_ops, const void *d, size_t s)
{
	(void) pmemops_xflush(p_ops, d, s, 0);
}

static force_inline void
pmemops_drain(const struct pmem_ops *p_ops)
{
	p_ops->drain(p_ops->base);
}

static force_inline void *
pmemops_memcpy(const struct pmem_ops *p_ops, void *dest,
		const void *src, size_t len, unsigned flags)
{
	return p_ops->memcpy(p_ops->base, dest, src, len, flags);
}

static force_inline void *
pmemops_memmove(const struct pmem_ops *p_ops, void *dest,
		const void *src, size_t len, unsigned flags)
{
	return p_ops->memmove(p_ops->base, dest, src, len, flags);
}

static force_inline void *
pmemops_memset(const struct pmem_ops *p_ops, void *dest, int c,
		size_t len, unsigned flags)
{
	return p_ops->memset(p_ops->base, dest, c, len, flags);
}

#ifdef __cplusplus
}
#endif

#endif
