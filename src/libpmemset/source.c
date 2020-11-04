// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

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
};

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

	*src = srcp;

	return 0;
}

#ifndef _WIN32
/*
 * pmemset_source_from_file -- initializes source structure and stores a path
 *                             to the file
 */
int
pmemset_source_from_file(struct pmemset_source **src, const char *file)
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
	srcp->file.path = strdup(file);

	if (srcp->file.path == NULL) {
		ERR("!strdup");
		Free(srcp);
		return PMEMSET_E_ERRNO;
	}

	*src = srcp;

	return 0;
}

/*
 * pmemset_source_from_temporary -- not supported
 */
int
pmemset_source_from_temporary(struct pmemset_source **src, const char *dir)
{
	return PMEMSET_E_NOSUPP;
}
#else
/*
 * pmemset_source_from_fileU -- initializes source structure and stores a path
 *                              to the file
 */
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
	*src = srcp;

	return 0;
}

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
 * pmemset_source_from_temporaryU -- not supported
 */
int
pmemset_source_from_temporaryU(struct pmemset_source **src, const char *dir)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_from_temporaryW -- not supported
 */
int
pmemset_source_from_temporaryW(struct pmemset_source **src, const wchar_t *dir)
{
	return PMEMSET_E_NOSUPP;
}
#endif

/*
 * pmemset_source_create_file_from_file - create pmemset_file based on file
 *                                        source type
 */
static int
pmemset_source_create_file_from_file(struct pmemset_source *src,
		struct pmemset_file **file, struct pmemset_config *cfg)
{
	return pmemset_file_from_file(file, src->file.path, cfg);
}

/*
 * pmemset_source_create_file_from_pmem2 - create pmemset_file based on pmem2
 *                                         source type
 */
static int
pmemset_source_create_file_from_pmem2(struct pmemset_source *src,
		struct pmemset_file **file, struct pmemset_config *cfg)
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

#ifdef _WIN32
/*
 * pmemset_source_file_extract - acquires file handle from the path stored in
 *                               the data source (windows)
 */
static int
pmemset_source_file_extract(const struct pmemset_source *src, HANDLE *handle)
{
	DWORD access = GENERIC_READ | GENERIC_WRITE;
	char *path = src->file.path;

	HANDLE h = CreateFile(path, access, 0, NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);
	if (h == INVALID_HANDLE_VALUE) {
		ERR("!CreateFile %s", path);
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	*handle = h;

	return 0;
}
#else
/*
 * pmemset_source_file_extract - acquires file descriptor from the path stored
 *                               int the data source (posix)
 */
static int
pmemset_source_file_extract(const struct pmemset_source *src, int *fd)
{
	int access = O_RDWR;
	char *path = src->file.path;

	int f = os_open(path, access);
	if (f < 0) {
		ERR("!open %s", path);
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	*fd = f;

	return 0;
}
#endif

#ifdef _WIN32
/*
 * pmemset_source_pmem2_extract - acquires file handle from
 *                                pmem2 source (windows)
 */
static int
pmemset_source_pmem2_extract(const struct pmemset_source *src, HANDLE *handle)
{
	HANDLE h;
	int ret = pmem2_source_get_handle(src->pmem2.src, &h);
	if (ret) {
		ERR("could not extract handle from provided source");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	*handle = h;

	return 0;
}
#else
/*
 * pmemset_source_pmem2_extract - acquires file descriptor from
 *                                pmem2 source (posix)
 */
static int
pmemset_source_pmem2_extract(const struct pmemset_source *src, int *fd)
{
	int f;
	int ret = pmem2_source_get_fd(src->pmem2.src, &f);
	if (ret) {
		ERR("could not extract file descriptor from provided source");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	*fd = f;

	return 0;
}
#endif

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
			struct pmemset_file **file, struct pmemset_config *cfg);
	void (*destroy)(struct pmemset_source **src);
#ifdef _WIN32
	int (*extract)(const struct pmemset_source *src, HANDLE *handle);
#else
	int (*extract)(const struct pmemset_source *src, int *fd);
#endif
	int (*validate)(const struct pmemset_source *src);
} pmemset_source_ops[MAX_PMEMSET_SOURCE_TYPE] = {
	[PMEMSET_SOURCE_FILE] = {
		.create_file = pmemset_source_create_file_from_file,
		.destroy = pmemset_source_file_destroy,
		.extract = pmemset_source_file_extract,
		.validate = pmemset_source_file_validate,
	},

	[PMEMSET_SOURCE_PMEM2] = {
		.create_file = pmemset_source_create_file_from_pmem2,
		.destroy = pmemset_source_empty_destroy,
		.extract = pmemset_source_pmem2_extract,
		.validate = pmemset_source_pmem2_validate,
	}
};

/*
 * pmemset_source_delete -- delete pmemset_source structure
 */
int
pmemset_source_delete(struct pmemset_source **src)
{
	enum pmemset_source_type type = (*src)->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	pmemset_source_ops[type].destroy(src);

	Free(*src);
	*src = NULL;
	return 0;
}

#ifdef _WIN32
/*
 * pmemset_source_extract -- extracts file handle from
 *                           data source (windows)
 */
int
pmemset_source_extract(struct pmemset_source *src, HANDLE *handle)
{
	enum pmemset_source_type type = src->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	return pmemset_source_ops[type].extract(src, handle);
}
#else
/*
 * pmemset_source_extract -- extracts file descriptor from
 *                           data source (posix)
 */
int
pmemset_source_extract(struct pmemset_source *src, int *fd)
{
	enum pmemset_source_type type = src->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	return pmemset_source_ops[type].extract(src, fd);
}
#endif

/*
 * pmemset_source_validate -- check the validity of created source
 */
int
pmemset_source_validate(const struct pmemset_source *src)
{
	enum pmemset_source_type type = src->type;
	if (type == PMEMSET_SOURCE_UNSPECIFIED ||
		type >= MAX_PMEMSET_SOURCE_TYPE) {
		ERR("invalid source type");
		return PMEMSET_E_INVALID_SOURCE_TYPE;
	}

	return pmemset_source_ops[type].validate(src);
}

/*
 * pmemset_source_create_pmemset_file -- create pmemset_file based on the type
 *                                       of pmemset_source
 */
int
pmemset_source_create_pmemset_file(struct pmemset_source *src,
		struct pmemset_file **file, struct pmemset_config *cfg)
{
	enum pmemset_source_type type = src->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	return pmemset_source_ops[type].create_file(src, file, cfg);
}
