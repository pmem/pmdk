// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * extent_linux.c - implementation of the linux fs extent query API
 */

#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

#include "libpmem2.h"
#include "pmem2_utils.h"

#include "file.h"
#include "out.h"
#include "extent.h"
#include "alloc.h"

/*
 * pmem2_extents_create_get -- allocate extents structure and get extents
 *                             of the given file
 */
int
pmem2_extents_create_get(int fd, struct extents **exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	ASSERT(fd > 2);
	ASSERTne(exts, NULL);

	enum pmem2_file_type pmem2_type;
	struct extents *pexts = NULL;
	struct fiemap *fmap = NULL;
	os_stat_t st;

	if (os_fstat(fd, &st) < 0) {
		ERR("!fstat %d", fd);
		return PMEM2_E_ERRNO;
	}

	int ret = pmem2_get_type_from_stat(&st, &pmem2_type);
	if (ret)
		return ret;

	/* directories do not have any extents */
	if (pmem2_type == PMEM2_FTYPE_DIR) {
		ERR(
			"checking extents does not make sense in case of directories");
		return PMEM2_E_INVALID_FILE_TYPE;
	}

	/* allocate extents structure */
	pexts = pmem2_zalloc(sizeof(struct extents), &ret);
	if (ret)
		return ret;

	/* save block size */
	LOG(10, "fd %i: block size: %li", fd, (long int)st.st_blksize);
	pexts->blksize = (uint64_t)st.st_blksize;

	/* DAX device does not have any extents */
	if (pmem2_type == PMEM2_FTYPE_DEVDAX) {
		*exts = pexts;
		return 0;
	}

	ASSERTeq(pmem2_type, PMEM2_FTYPE_REG);

	fmap = pmem2_zalloc(sizeof(struct fiemap), &ret);
	if (ret)
		goto error_free;

	fmap->fm_start = 0;
	fmap->fm_length = (size_t)st.st_size;
	fmap->fm_flags = 0;
	fmap->fm_extent_count = 0;
	fmap->fm_mapped_extents = 0;

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0) {
		ERR("!fiemap ioctl() for fd=%d failed", fd);
		ret = PMEM2_E_ERRNO;
		goto error_free;
	}

	size_t newsize = sizeof(struct fiemap) +
		fmap->fm_mapped_extents * sizeof(struct fiemap_extent);

	struct fiemap *newfmap = pmem2_realloc(fmap, newsize, &ret);
	if (ret)
		goto error_free;

	fmap = newfmap;
	memset(fmap->fm_extents, 0, fmap->fm_mapped_extents *
					sizeof(struct fiemap_extent));
	fmap->fm_extent_count = fmap->fm_mapped_extents;
	fmap->fm_mapped_extents = 0;

	if (ioctl(fd, FS_IOC_FIEMAP, fmap) != 0) {
		ERR("!fiemap ioctl() for fd=%d failed", fd);
		ret = PMEM2_E_ERRNO;
		goto error_free;
	}

	LOG(4, "file with fd=%i has %u extents:", fd, fmap->fm_mapped_extents);

	/* save number of extents */
	pexts->extents_count = fmap->fm_mapped_extents;

	pexts->extents = pmem2_malloc(
				pexts->extents_count * sizeof(struct extent),
				&ret);
	if (ret)
		goto error_free;

	/* save extents */
	unsigned e;
	for (e = 0; e < fmap->fm_mapped_extents; e++) {
		pexts->extents[e].offset_physical =
			fmap->fm_extents[e].fe_physical;
		pexts->extents[e].offset_logical =
			fmap->fm_extents[e].fe_logical;
		pexts->extents[e].length =
			fmap->fm_extents[e].fe_length;

		LOG(10, "   #%u: off_phy: %lu off_log: %lu len: %lu",
			e,
			pexts->extents[e].offset_physical,
			pexts->extents[e].offset_logical,
			pexts->extents[e].length);
	}

	*exts = pexts;

	Free(fmap);

	return 0;

error_free:
	Free(pexts->extents);
	Free(pexts);
	Free(fmap);

	return ret;
}

/*
 * pmem2_extents_destroy -- free extents structure
 */
void
pmem2_extents_destroy(struct extents **exts)
{
	LOG(3, "extents %p", exts);

	ASSERTne(exts, NULL);

	if (*exts) {
		Free((*exts)->extents);
		Free(*exts);
		*exts = NULL;
	}
}
