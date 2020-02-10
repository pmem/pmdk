/*
 * Copyright 2020, Intel Corporation
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

#include "source.h"
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2.h"
#include "pmem2_utils.h"

int
pmem2_source_from_anon(struct pmem2_source **src)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_delete(struct pmem2_source **src)
{
	Free(*src);
	*src = NULL;
	return 0;
}

#ifndef _WIN32
int
pmem2_source_device_id(const struct pmem2_source *cfg,
	char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#else
int
pmem2_source_device_idW(const struct pmem2_source *cfg,
	wchar_t *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_source_device_idU(const struct pmem2_source *cfg,
	char *id, size_t *len)
{
	return PMEM2_E_NOSUPP;
}
#endif

int
pmem2_source_device_usc(const struct pmem2_source *cfg, uint64_t *usc)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_iterator_new(const struct pmem2_source *cfg,
		struct pmem2_badblock_iterator **pbb)
{
	return PMEM2_E_NOSUPP;
}

int
pmem2_badblock_next(struct pmem2_badblock_iterator *pbb,
		struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}

void pmem2_badblock_iterator_delete(
		struct pmem2_badblock_iterator **pbb)
{
}

int
pmem2_badblock_clear(const struct pmem2_source *cfg,
		const struct pmem2_badblock *bb)
{
	return PMEM2_E_NOSUPP;
}
