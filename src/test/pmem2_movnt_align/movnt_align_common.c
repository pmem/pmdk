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

/*
 * movnt_align_common.c -- common part for tests doing a persistent movnt align
 */

#include "unittest.h"
#include "movnt_align_common.h"

char *Src;
char *Dst;
char *Scratch;

/*
 * check_memmove -- invoke check function with pmem_memmove_persist
 */
void
check_memmove(size_t doff, size_t soff, size_t len, pmem_memmove_fn fn,
		unsigned flags)
{
	memset(Dst + doff, 1, len);
	memset(Src + soff, 0, len);

	fn(Dst + doff, Src + soff, len, flags);

	if (memcmp(Dst + doff, Src + soff, len))
		UT_FATAL("memcpy/memmove failed");
}

/*
 * check_memmove -- invoke check function with pmem_memcpy_persist
 */
void
check_memcpy(size_t doff, size_t soff, size_t len, pmem_memcpy_fn fn,
		unsigned flags)
{
	memset(Dst, 2, N_BYTES);
	memset(Src, 3, N_BYTES);
	memset(Scratch, 2, N_BYTES);

	memset(Dst + doff, 1, len);
	memset(Src + soff, 0, len);
	memcpy(Scratch + doff, Src + soff, len);

	fn(Dst + doff, Src + soff, len, flags);

	if (memcmp(Dst, Scratch, N_BYTES))
		UT_FATAL("memcpy/memmove failed");
}

/*
 * check_memset -- check pmem_memset_no_drain function
 */
void
check_memset(size_t off, size_t len, pmem_memset_fn fn, unsigned flags)
{
	memset(Scratch, 2, N_BYTES);
	memset(Scratch + off, 1, len);

	memset(Dst, 2, N_BYTES);
	fn(Dst + off, 1, len, flags);

	if (memcmp(Dst, Scratch, N_BYTES))
		UT_FATAL("memset failed");
}

unsigned Flags[] = {
		0,
		PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_NONTEMPORAL,
		PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_WC,
		PMEM_F_MEM_WB,
		PMEM_F_MEM_NOFLUSH,
		/* all possible flags */
		PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
			PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
			PMEM_F_MEM_WC | PMEM_F_MEM_WB,
};
