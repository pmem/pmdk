/*
 * Copyright 2017, Intel Corporation
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
 * os_ras_linux.c -- Linux ras abstraction layer
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "os.h"
#include "os_dimm.h"
#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>

/*
 * http://pmem.io/documents/NVDIMM_DSM_Interface-V1.6.pdf
 * Table 3-2 SMART amd Health Data - Validity flags
 * Bit[5] – If set to 1, indicates that Unsafe Shutdown Count
 * field is valid
 */
#define USC_VALID_FLAG (1 << 5)

#define FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns)	\
	ndctl_bus_foreach(ctx, bus)				\
		ndctl_region_foreach(bus, region)		\
			ndctl_namespace_foreach(region, ndns)	\

/*
 * os_dimm_interleave_set -- (internal) returns set of dimms
 * where the pool is located
 */
static struct ndctl_interleave_set *
os_dimm_interleave_set(struct ndctl_ctx *ctx, os_stat_t st)
{
	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	dev_t dev = S_ISCHR(st.st_mode) ? st.st_rdev : st.st_dev;

	FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns) {
		struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
		struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
		struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
		const char *devname;

		if (btt) {
			devname = ndctl_btt_get_block_device(btt);
		} else if (pfn) {
			devname = ndctl_pfn_get_block_device(pfn);
		} else if (dax) {
			struct daxctl_region *dax_region;
			dax_region = ndctl_dax_get_daxctl_region(dax);
			/* there is always one dax device in dax_region */
			if (dax_region) {
				struct daxctl_dev *dev;
				dev = daxctl_dev_get_first(dax_region);
				devname = daxctl_dev_get_devname(dev);
			} else {
				return NULL;
			}
		} else {
			devname = ndctl_namespace_get_block_device(ndns);
		}

		if (*devname == '\0')
			continue;

		char *path;
		os_stat_t stat;
		if (asprintf(&path, "/dev/%s", devname) == -1)
			return NULL;

		if (os_stat(path, &stat)) {
			free(path);
			return NULL;
		}

		free(path);

		if (dev == stat.st_rdev) {
			return ndctl_region_get_interleave_set(region);
		}
	}
	return NULL;
}

/*
 * os_dimm_uid -- returns a file uid based on dimms uids
 */
int
os_dimm_uid(const char *path, char *uid)
{
	os_stat_t st;

	struct ndctl_ctx *ctx;
	struct ndctl_interleave_set *set;
	struct ndctl_dimm *dimm;

	if (os_stat(path, &st))
		return -1;
	if (ndctl_new(&ctx))
		return -1;
	set = os_dimm_interleave_set(ctx, st);

	if (set != NULL) {
		ndctl_dimm_foreach_in_interleave_set(set, dimm) {
			const char *dimm_uid = ndctl_dimm_get_unique_id(dimm);
			strcat(uid, dimm_uid);
		}
	}

	ndctl_unref(ctx);
	return 0;
}

/*
 * os_dimm_uid_size -- returns uuid length for given file
 */
int
os_dimm_uid_size(const char *path, size_t *len)
{
	os_stat_t st;

	struct ndctl_ctx *ctx;
	struct ndctl_interleave_set *set;
	struct ndctl_dimm *dimm;

	if (os_stat(path, &st))
		return -1;

	if (ndctl_new(&ctx))
		return -1;

	set = os_dimm_interleave_set(ctx, st);
	*len = sizeof('\0');
	if (set != NULL) {
		ndctl_dimm_foreach_in_interleave_set(set, dimm) {
			*len += strlen(ndctl_dimm_get_unique_id(dimm));
		}
	}
	ndctl_unref(ctx);
	return 0;
}

/*
 * os_dimm_usc -- returns unsafe shutdown count
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	os_stat_t st;
	struct ndctl_ctx *ctx;

	*usc = 1;

	if (os_stat(path, &st))
		return -1;

	if (ndctl_new(&ctx))
		return -1;

	struct ndctl_interleave_set *iset =
		os_dimm_interleave_set(ctx, st);

	if (iset == NULL)
		goto out;

	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach_in_interleave_set(iset, dimm) {
		struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_smart(dimm);

		if (ndctl_cmd_submit(cmd))
			goto out;

		if (ndctl_cmd_smart_get_flags(cmd) & USC_VALID_FLAG)
			goto out;

		*usc += ndctl_cmd_smart_get_shutdown_count(cmd);
	}
out:
	ndctl_unref(ctx);
	return 0;
}
