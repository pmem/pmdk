/*
 * Copyright 2014-2017, Intel Corporation
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
 * info_cto.c -- pmempool info command source file for cto pool
 */
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <assert.h>
#include <inttypes.h>

#include "common.h"
#include "output.h"
#include "info.h"

/*
 * info_cto_descriptor -- print pmemcto descriptor
 */
static void
info_cto_descriptor(struct pmem_info *pip)
{
	int v = VERBOSE_DEFAULT;

	if (!outv_check(v))
		return;

	outv(v, "\nPMEM CTO Header:\n");
	struct pmemcto *pcp = pip->cto.pcp;

	uint8_t *hdrptr = (uint8_t *)pcp + sizeof(pcp->hdr);
	size_t hdrsize = sizeof(*pcp) - sizeof(pcp->hdr);
	size_t hdroff = sizeof(pcp->hdr);
	outv_hexdump(pip->args.vhdrdump, hdrptr, hdrsize, hdroff, 1);

	/* check if layout is zeroed */
	char *layout = util_check_memory((uint8_t *)pcp->layout,
			sizeof(pcp->layout), 0) ?
					pcp->layout : "(null)";

	outv_field(v, "Layout", "%s", layout);
	outv_field(v, "Base address", "%p", (void *)pcp->addr);
	outv_field(v, "Size", "0x%zx", (size_t)pcp->size);
	outv_field(v, "Consistent", "%d", pcp->consistent);
	outv_field(v, "Root pointer", "%p", (void *)pcp->root);
}

/*
 * pmempool_info_cto -- print information about cto pool type
 */
int
pmempool_info_cto(struct pmem_info *pip)
{
	pip->cto.pcp = pool_set_file_map(pip->pfile, 0);
	if (pip->cto.pcp == NULL)
		return -1;

	pip->cto.size = pip->pfile->size;

	info_cto_descriptor(pip);

	return 0;
}
