// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * part.c -- implementation of common part API
 */

#include <fcntl.h>

#include "alloc.h"
#include "libpmemset.h"
#include "os.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "source.h"

struct pmemset_part {
	struct pmemset *set;
	struct pmemset_source *src;
	size_t offset;
	size_t length;
	union {
#ifdef _WIN32
		HANDLE handle;
#else
		int fd;
#endif
	};
};

/*
 * pmemset_part_new -- creates a new part for the provided set
 */
int
pmemset_part_new(struct pmemset_part **part, struct pmemset *set,
		struct pmemset_source *src, size_t offset, size_t length)
{
	LOG(3, "part %p set %p src %p offset %zu length %zu",
			part, set, src, offset, length);
	PMEMSET_ERR_CLR();

	int ret;
	struct pmemset_part *partp;
	*part = NULL;

	ret = pmemset_source_validate(src);
	if (ret)
		return ret;

	partp = pmemset_malloc(sizeof(*partp), &ret);
	if (ret)
		return ret;

	ASSERTne(partp, NULL);

#ifdef _WIN32
	ret = pmemset_source_extract(src, &partp->handle);
#else
	ret = pmemset_source_extract(src, &partp->fd);
#endif
	if (ret)
		goto err_free_part;

	partp->set = set;
	partp->src = src;
	partp->offset = offset;
	partp->length = length;

	*part = partp;

	return 0;

err_free_part:
	Free(partp);
	return ret;
}

/*
 * memset_part_pread_mcsafe -- not supported
 */
int
pmemset_part_pread_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_pwrite_mcsafe -- not supported
 */
int
pmemset_part_pwrite_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map -- not supported
 */
int
pmemset_part_map(struct pmemset_part **part, struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_drop -- not supported
 */
int
pmemset_part_map_drop(struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_descriptor -- not supported
 */
struct pmemset_part_descriptor
pmemset_part_map_descriptor(struct pmemset_part_map *pmap)
{
	struct pmemset_part_descriptor desc;
	/* compiler is crying when struct is uninitialized */
	desc.addr = NULL;
	desc.size = 0;
	return desc;
}

/*
 * pmemset_part_map_first -- not supported
 */
int
pmemset_part_map_first(struct pmemset *set, struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_next -- not supported
 */
int
pmemset_part_map_next(struct pmemset *set, struct pmemset_part_map **pmap)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_map_by_address -- not supported
 */
int
pmemset_part_map_by_address(struct pmemset *set, struct pmemset_part **part,
		void *addr)
{
	return PMEMSET_E_NOSUPP;
}

#ifdef _WIN32
/*
 * pmemset_part_file_close -- closes a file handle stored in a part (windows).
 * XXX: created for testing purposes, should be deleted after implementing a
 * function that closes the file handle stored in part.
 */
void
pmemset_part_file_close(const struct pmemset_part *part)
{
	CloseHandle(part->handle);
}
#else
/*
 * pmemset_part_file_close -- closes a file descriptor stored in a part (posix).
 * XXX: created for testing purposes, should be deleted after implementing a
 * function that closes the file handle stored in part.
 */
void
pmemset_part_file_close(const struct pmemset_part *part)
{
	os_close(part->fd);
}
#endif
