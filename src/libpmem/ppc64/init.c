/*
 * Copyright 2019, IBM Corporation
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

#include <libpmem.h>
#include "out.h"
#include "pmem.h"

static void ppc_predrain_fence(void)
{
	LOG(15, NULL);
}

static void ppc_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, size);

}

static void ppc_deep_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, size);
}

const static struct pmem_funcs ppc64_pmem_funcs = {
	.predrain_fence = ppc_predrain_fence,
	.flush = ppc_flush,
	.deep_flush = ppc_deep_flush,
	.is_pmem = is_pmem_detect,
	.memmove_nodrain = memmove_nodrain_generic,
	.memset_nodrain = memset_nodrain_generic,
};

/*
 * Provide arch specific implementation for pmem function
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	LOG(3, "libpmem: PPC64 support");
	LOG(3, "PMDK PPC64 support currently is for testing only");
	LOG(3, "Please dont use this library in production environment");
	*funcs = ppc64_pmem_funcs;
}
