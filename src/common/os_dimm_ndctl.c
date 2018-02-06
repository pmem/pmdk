/*
 * Copyright 2017-2018, Intel Corporation
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
 * os_dimm_ndctl.c -- read dimm stats by ndctl
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
#include "out.h"
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
 * where the pool file is located
 */
static struct ndctl_interleave_set *
os_dimm_interleave_set(struct ndctl_ctx *ctx, os_stat_t st)
{
	LOG(3, "ctx %p", ctx);

	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	dev_t dev = S_ISCHR(st.st_mode) ? st.st_rdev : st.st_dev;

	FOREACH_BUS_REGION_NAMESPACE(ctx, bus, region, ndns) {
		struct ndctl_btt *btt;
		struct ndctl_dax *dax;
		struct ndctl_pfn *pfn;
		const char *devname;

		if ((btt = ndctl_namespace_get_btt(ndns))) {
			devname = ndctl_btt_get_block_device(btt);
		} else if ((pfn = ndctl_namespace_get_pfn(ndns))) {
			devname = ndctl_pfn_get_block_device(pfn);
		} else if ((dax = ndctl_namespace_get_dax(ndns))) {
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

		char path[PATH_MAX];
		os_stat_t stat;
		if (sprintf(path, "/dev/%s", devname) == -1)
			return NULL;

		if (os_stat(path, &stat)) {
			return NULL;
		}

		if (dev == stat.st_rdev) {
			return ndctl_region_get_interleave_set(region);
		}
	}
	return NULL;
}

/*
 * os_dimm_uid -- returns a file uid based on dimms uids
 *
 * if uid == null then function will return required buffer size
 */
int
os_dimm_uid(const char *path, char *uid, size_t *buff_len)
{
	LOG(3, "path %s, uid %p, len %lu", path, uid, *buff_len);

	os_stat_t st;

	struct ndctl_ctx *ctx;
	struct ndctl_interleave_set *set;
	struct ndctl_dimm *dimm;
	int ret = 0;
	if (os_stat(path, &st))
		return -1;

	if (ndctl_new(&ctx))
		return -1;

	if (uid == NULL) {
		*buff_len = 1; /* '\0' */
	}

	set = os_dimm_interleave_set(ctx, st);
	if (set == NULL)
		goto end;

	if (uid == NULL) {
		ndctl_dimm_foreach_in_interleave_set(set, dimm) {
			*buff_len += strlen(ndctl_dimm_get_unique_id(dimm));
		}
		goto end;
	}
	size_t len = 1;
	ndctl_dimm_foreach_in_interleave_set(set, dimm) {
		const char *dimm_uid = ndctl_dimm_get_unique_id(dimm);
		len += strlen(dimm_uid);
		if (len > *buff_len) {
			ret = -1;
			goto end;
		}
		strncat(uid, dimm_uid, *buff_len);
	}
end:
	ndctl_unref(ctx);
	return ret;
}

/*
 * os_dimm_usc -- returns unsafe shutdown count
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	LOG(3, "path %s, uid %p", path, usc);

	os_stat_t st;
	struct ndctl_ctx *ctx;

	*usc = 0;

	if (os_stat(path, &st)) {
		ERR("!stat: %s", path);
		return -1;
	}

	if (ndctl_new(&ctx)) {
		ERR("!ndctl_new");
		return -1;
	}

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
