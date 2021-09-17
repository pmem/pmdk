// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libpmem2.h>
#include <string.h>

#include "libpmemset.h"
#include "libpmem2.h"

#include "alloc.h"
#include "file.h"
#include "os.h"
#include "os_thread.h"
#include "pmemset_utils.h"
#include "ravl.h"
#include "sds.h"
#include "source.h"
#include "sys_util.h"

struct pmemset_source {
	enum pmemset_source_type type;
	union {
		struct {
			char *path;
		} file;
		struct {
			struct pmem2_source *src;
		} pmem2;
		struct {
			char *dir;
		} temp;
	};
	struct pmemset_file *file_set;
	struct {
		struct pmemset_sds *sds;
		enum pmemset_part_state *state;
		struct pmemset_badblock *bb;
		int use_count;
	} extras;
};

/*
 * pmemset_source_open_file -- validate and create source from file
 */
static int
pmemset_source_open_file(struct pmemset_source *srcp, uint64_t flags)
{
	int ret;

	/* validate only for cases without flags (internal calls) */
	if (flags == 0) {
		ret = pmemset_source_validate(srcp);
		if (ret)
			goto end;
	}

	ret = pmemset_source_create_pmemset_file(srcp, &srcp->file_set, flags);

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
	srcp->extras.sds = NULL;
	srcp->extras.bb = NULL;
	srcp->extras.state = NULL;
	srcp->extras.use_count = 0;

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
				uint64_t flags)
{
	LOG(3, "src %p file %s flags 0x%" PRIx64, src, file, flags);
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!file) {
		ERR("file path cannot be empty");
		return PMEMSET_E_INVALID_SOURCE_PATH;
	}

	if (flags & ~PMEMSET_SOURCE_FILE_CREATE_VALID_FLAGS) {
		ERR("pmemset_xsource_from_fileU invalid flags 0x%" PRIx64,
			flags);
		return PMEMSET_E_INVALID_SOURCE_FILE_CREATE_FLAGS;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_FILE;
	srcp->file.path = Strdup(file);
	srcp->extras.sds = NULL;
	srcp->extras.bb = NULL;
	srcp->extras.state = NULL;
	srcp->extras.use_count = 0;

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

	return pmemset_xsource_from_fileU(src, file, 0);
}

/*
 * pmemset_source_from_temporaryU -- create source using temp file from the dir
 */
#ifndef _WIN32
static inline
#endif
int
pmemset_source_from_temporaryU(struct pmemset_source **src, const char *dir)
{
	LOG(3, "src %p dir %s", src, dir);
	PMEMSET_ERR_CLR();

	*src = NULL;

	if (!dir) {
		ERR("directory path cannot be empty");
		return PMEMSET_E_INVALID_SOURCE_PATH;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_TEMP;
	srcp->temp.dir = Strdup(dir);
	srcp->extras.sds = NULL;
	srcp->extras.bb = NULL;
	srcp->extras.state = NULL;
	srcp->extras.use_count = 0;

	if (srcp->temp.dir == NULL) {
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
			uint64_t flags)
{
	return pmemset_xsource_from_fileU(src, file, flags);
}

/*
 * pmemset_source_from_temporary -- create source using temp file from the dir
 */
int
pmemset_source_from_temporary(struct pmemset_source **src, const char *dir)
{
	return pmemset_source_from_temporaryU(src, dir);
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
pmemset_source_from_temporaryW(struct pmemset_source **src, const wchar_t *dir)
{
	const char *udir = util_toUTF8(dir);
	return pmemset_source_from_temporaryU(src, udir);
}
#endif

/*
 * pmemset_source_create_file_from_file - create pmemset_file based on file
 *                                        source type
 */
static int
pmemset_source_create_file_from_file(struct pmemset_source *src,
		struct pmemset_file **file, uint64_t flags)
{
	return pmemset_file_from_file(file, src->file.path, flags);
}

/*
 * pmemset_source_create_file_from_pmem2 - create pmemset_file based on pmem2
 *                                         source type
 */
static int
pmemset_source_create_file_from_pmem2(struct pmemset_source *src,
		struct pmemset_file **file, uint64_t flags)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(flags);

	return pmemset_file_from_pmem2(file, src->pmem2.src);
}

/*
 * pmemset_source_create_file_from_temp - create pmemset_file based on temp
 *                                        source type
 */
static int
pmemset_source_create_file_from_temp(struct pmemset_source *src,
		struct pmemset_file **file, uint64_t flags)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(flags);

	return pmemset_file_from_dir(file, src->temp.dir);
}

/*
 * pmemset_source_empty_destroy - empty destroy function
 */
static void
pmemset_source_empty_destroy(struct pmemset_source **src)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(src);
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
			return PMEMSET_E_INVALID_SOURCE_PATH;
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
			struct pmemset_file **file, uint64_t flags);
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
	},

	[PMEMSET_SOURCE_TEMP] = {
		.create_file = pmemset_source_create_file_from_temp,
		.destroy = pmemset_source_file_destroy,
		.validate = pmemset_source_file_validate,
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

	struct pmemset_source *source = *src;

	enum pmemset_source_type type = source->type;
	ASSERTne(type, PMEMSET_SOURCE_UNSPECIFIED);

	struct pmemset_file *f = pmemset_source_get_set_file(source);
	pmemset_file_delete(&f);

	pmemset_source_ops[type].destroy(src);

	if (source->extras.sds) {
		int ret = pmemset_sds_delete(&source->extras.sds);
		ASSERTeq(ret, 0);
	}

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
		struct pmemset_file **file, uint64_t flags)
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

/*
 * pmemset_source_set_sds -- sets SDS in the source structure
 */
int
pmemset_source_set_sds(struct pmemset_source *src, struct pmemset_sds *sds,
		enum pmemset_part_state *state_ptr)
{
	LOG(3, "src %p sds %p state %p", src, sds, state_ptr);

	if (src->extras.sds) {
		ERR("sds %p is already set in the source %p", sds, src);
		return PMEMSET_E_SDS_ALREADY_SET;
	}

	struct pmemset_sds *sds_copy = NULL;
	int ret = pmemset_sds_duplicate(&sds_copy, sds);
	if (ret)
		return ret;

	src->extras.sds = sds_copy;
	src->extras.state = state_ptr;

	return 0;
}

/*
 * pmemset_source_get_sds -- gets SDS from the source structure
 */
struct pmemset_sds *
pmemset_source_get_sds(struct pmemset_source *src)
{
	return src->extras.sds;
}

/*
 * pmemset_source_get_use_count -- retrieve current use count (number of parts
 *                                 mapped from this source currenly in use)
 */
int
pmemset_source_get_use_count(struct pmemset_source *src)
{
	return src->extras.use_count;
}

/*
 * pmemset_source_increment_use_count -- increment source use count by 1
 */
void
pmemset_source_increment_use_count(struct pmemset_source *src)
{
	src->extras.use_count++;
}

/*
 * pmemset_source_decrement_use_count -- decrement source use count by 1
 */
void
pmemset_source_decrement_use_count(struct pmemset_source *src)
{
	ASSERTne(src->extras.use_count, 0);
	src->extras.use_count--;
}

/*
 * pmemset_source_get_state_ptr -- gets part state pointer from the source
 *                                 structure
 */
enum pmemset_part_state *
pmemset_source_get_part_state_ptr(struct pmemset_source *src)
{
	return src->extras.state;
}
