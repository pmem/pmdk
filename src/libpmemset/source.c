// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include <errno.h>
#include <fcntl.h>
#include <libpmem2.h>
#include <string.h>

#include "libpmemset.h"
#include "libpmem2.h"

#include "alloc.h"
#include "file.h"
#include "os.h"
#include "pmemset_utils.h"
#include "source.h"

#define PMEMSET_SOURCE_FILE_CREATE_DISPOSITION_FLAGS\
		(PMEMSET_SOURCE_FILE_CREATE_ALWAYS|\
		PMEMSET_SOURCE_FILE_CREATE_IF_NEEDED)

struct pmemset_source {
	enum pmemset_source_type type;
	union {
		struct {
			char *path;
		} file;
		struct {
			struct pmem2_source *src;
		} pmem2;
	};
	struct pmemset_file *file_set;
};

/*
 * pmemset_source_open_file -- validate and create source from file
 */
static int
pmemset_source_open_file(struct pmemset_source *srcp, unsigned flags)
{
	int ret;

	if ((flags & PMEMSET_SOURCE_FILE_CREATE_DISPOSITION_FLAGS) == 0) {
		ret = pmemset_source_validate(srcp);
		if (ret)
			goto end;
	}

	ret = pmemset_source_create_pmemset_file(srcp, &srcp->file_set, flags);
	if (ret)
		goto end;
end:
	return ret;
}

/*
 * pmemset_source_from_pmem2 -- create pmemset source using source from pmem2
 */
int
pmemset_source_from_pmem2(struct pmemset_source **src,
		struct pmem2_source *pmem2_src)
{
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!pmem2_src) {
		ERR("pmem2_source cannot be NULL");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	ASSERTne(srcp, NULL);

	srcp->type = PMEMSET_SOURCE_PMEM2;
	srcp->pmem2.src = pmem2_src;

	ret = pmemset_source_open_file(srcp, 0);
	if (ret)
		goto free_srcp;

	*src = srcp;

	return 0;

free_srcp:
	Free(srcp);
	return ret;
}

/*
 * pmemset_source_from_fileU -- initializes source structure and stores a path
 *                              to the file
 */
#ifndef _WIN32
static inline
#endif
int
pmemset_source_from_fileU(struct pmemset_source **src, const char *file)
{
	LOG(3, "src %p file %s", src, file);
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!file) {
		ERR("file path cannot be empty");
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_FILE;
	srcp->file.path = Strdup(file);

	if (srcp->file.path == NULL) {
		ERR("!strdup");
		Free(srcp);
		return PMEMSET_E_ERRNO;
	}

	ret = pmemset_source_open_file(srcp, 0);
	if (ret)
		goto free_srcp;

	*src = srcp;

	return 0;

free_srcp:
	Free(srcp);
	return ret;
}

/*
 * pmemset_xsource_from_fileU -- initializes source structure and stores a path
 *                              to the file
 */
#ifndef _WIN32
static inline
#endif
int
pmemset_xsource_from_fileU(struct pmemset_source **src, const char *file,
				unsigned flags)
{
	LOG(3, "src %p file %s flags %u", src, file, flags);
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!file) {
		ERR("file path cannot be empty");
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	if (flags & ~PMEMSET_SOURCE_FILE_CREATE_VALID_FLAGS) {
		ERR("pmemset_xsource_from_fileU invalid flags 0x%x", flags);
		return PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_FILE;
	srcp->file.path = Strdup(file);

	if (srcp->file.path == NULL) {
		ERR("!strdup");
		Free(srcp);
		return PMEMSET_E_ERRNO;
	}

	ret = pmemset_source_open_file(srcp, flags);
	if (ret)
		goto free_srcp;

	*src = srcp;

	return 0;

free_srcp:
	Free(srcp);
	return ret;
}

/*
 * pmemset_source_from_temporaryU -- create source using temp file from the dir
 */
#ifndef _WIN32
static inline
#endif
int
pmemset_source_from_temporaryU(struct pmemset_source **src, const char *dir,
		size_t len)
{
	return PMEMSET_E_NOSUPP;
}

#ifndef _WIN32
/*
 * pmemset_source_from_file -- initializes source structure and stores a path
 *                             to the file
 */
int
pmemset_source_from_file(struct pmemset_source **src, const char *file)
{
	return pmemset_source_from_fileU(src, file);
}

/*
 * pmemset_xsource_from_file -- initializes source structure and stores a path
 *                             to the file
 */
int
pmemset_xsource_from_file(struct pmemset_source **src, const char *file,
			unsigned flags)
{
	return pmemset_xsource_from_fileU(src, file, flags);
}

/*
 * pmemset_source_from_temporary -- create source using temp file from the dir
 */
int
pmemset_source_from_temporary(struct pmemset_source **src, const char *dir,
		size_t len)
{
	return pmemset_source_from_temporaryU(src, dir, len);
}
#else

/*
 * pmemset_source_from_fileW -- initializes source structure and stores a path
 *                              to the file
 */
int
pmemset_source_from_fileW(struct pmemset_source **src, const wchar_t *file)
{
	const char *ufile = util_toUTF8(file);
	return pmemset_source_from_fileU(src, ufile);
}

/*
 * pmemset_xsource_from_fileW -- initializes source structure and stores a path
 *                              to the file
 */
int
pmemset_xsource_from_fileW(struct pmemset_source **src, const wchar_t *file,
				unsigned flags)
{
	const char *ufile = util_toUTF8(file);
	return pmemset_xsource_from_fileU(src, ufile, flags);
}

/*
 * pmemset_source_from_temporaryW -- create source using temp file from the dir
 */
int
pmemset_source_from_temporaryW(struct pmemset_source **src, const wchar_t *dir,
		size_t len)
{
	const char *udir = util_toUTF8(dir);
	return pmemset_source_from_temporaryU(src, udir, len);
}
#endif

/*
 * pmemset_source_create_file_from_file - create pmemset_file based on file
 *                                        source type
 */
static int
pmemset_source_create_file_from_file(struct pmemset_source *src,
		struct pmemset_file **file, unsigned flags)
{
	return pmemset_file_from_file(file, src->file.path, flags);
}

/*
 * pmemset_source_create_file_from_pmem2 - create pmemset_file based on pmem2
 *                                         source type
 */
static int
pmemset_source_create_file_from_pmem2(struct pmemset_source *src,
		struct pmemset_file **file, unsigned flags)
{
	return pmemset_file_from_pmem2(file, src->pmem2.src);
}

/*
 * pmemset_source_empty_destroy - empty destroy function
 */
static void
pmemset_source_empty_destroy(struct pmemset_source **src)
{
	;
}

/*
 * pmemset_source_file_destroy -- delete source's filepath member
 */
static void
pmemset_source_file_destroy(struct pmemset_source **src)
{
	Free((*src)->file.path);
}

/*
 * pmemset_source_file_validate - check the validity of source created
 *                                from file
 */
static int
pmemset_source_file_validate(const struct pmemset_source *src)
{
	os_stat_t stat;
	if (os_stat(src->file.path, &stat) < 0) {
		if (errno == ENOENT) {
			ERR("invalid path specified in the source");
			return PMEMSET_E_INVALID_FILE_PATH;
		}
		ERR("!stat");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}

/*
 * pmemset_source_pmem2_validate - check the validity of source created
 *                                 from pmem2 source
 */
static int
pmemset_source_pmem2_validate(const struct pmemset_source *src)
{
	if (!src->pmem2.src) {
		ERR("invalid pmem2_source specified in the data source");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	return 0;
}

static const struct {
	int (*create_file)(struct pmemset_source *src,
			struct pmemset_file **file, unsigned flags);
	void (*destroy)(struct pmemset_source **src);
	int (*validate)(const struct pmemset_source *src);
} pmemset_source_ops[MAX_PMEMSET_SOURCE_TYPE] = {
	[PMEMSET_SOURCE_FILE] = {
		.create_file = pmemset_source_create_file_from_file,
		.destroy = pmemset_source_file_destroy,
		.validate = pmemset_source_file_validate,
	},

	[PMEMSET_SOURCE_PMEM2] = {
		.create_file = pmemset_source_create_file_from_pmem2,
		.destroy = pmemset_source_empty_destroy,
		.validate = pmemset_source_pmem2_validate,
	}
};

/*
 * pmemset_source_delete -- delete pmemset_source structure
 */
int
pmemset_source_delete(struct pmemset_source **src)
{
	if (*src == NULL)
		return 0;

	enum pmemset_source_type type = (*src)->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	struct pmemset_file *f = pmemset_source_get_set_file(*src);
	pmemset_file_delete(&f);

	pmemset_source_ops[type].destroy(src);

	Free(*src);
	*src = NULL;
	return 0;
}

/*
 * pmemset_source_validate -- check the validity of created source
 */
int
pmemset_source_validate(const struct pmemset_source *src)
{
	enum pmemset_source_type type = src->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	return pmemset_source_ops[type].validate(src);
}

/*
 * pmemset_source_create_pmemset_file -- create pmemset_file based on the type
 *                                       of pmemset_source
 */
int
pmemset_source_create_pmemset_file(struct pmemset_source *src,
		struct pmemset_file **file, unsigned flags)
{
	enum pmemset_source_type type = src->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	return pmemset_source_ops[type].create_file(src, file, flags);
}

/*
 * pmemset_source_get_set_file -- returns pointer to pmemset_file from
 * pmemset_source structure
 */
struct pmemset_file *
pmemset_source_get_set_file(struct pmemset_source *src)
{
	return src->file_set;
}
