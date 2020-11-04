// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * source.c -- implementation of common config API
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "libpmemset.h"
#include "libpmem2.h"

#include "os.h"
#include "pmemset_utils.h"
#include "source.h"

struct pmemset_source {
	enum pmemset_source_type type;
	union {
		char *filepath;
		struct pmem2_source *pmem2_src;
	} value;
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
	srcp->value.pmem2_src = pmem2_src;

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

	if (!file) {
		ERR("file path cannot be empty");
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_PATH;
	srcp->value.filepath = strdup(file);

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

	if (!file) {
		ERR("file path cannot be empty");
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	int ret;
	struct pmemset_source *srcp = pmemset_malloc(sizeof(**src), &ret);
	if (ret)
		return ret;

	srcp->type = PMEMSET_SOURCE_PATH;
	srcp->value.filepath = strdup(file);

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
 * pmemset_source_delete -- not supported
 */
int
pmemset_source_delete(struct pmemset_source **src)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_source_get_filepath -- get file path from provided source
 */
int
pmemset_source_get_filepath(const struct pmemset_source *src, char **filepath)
{
	LOG(3, "src type %d", src->type);
	PMEMSET_ERR_CLR();

	if (src->type != PMEMSET_SOURCE_PATH) {
		ERR("filepath is not set");
		return PMEMSET_E_INVALID_FILE_PATH;
	}

	*filepath = src->value.filepath;

	return 0;
}

/*
 * pmemset_source_get_pmem2_source -- get pmem2_source from provided source
 */
int
pmemset_source_get_pmem2_source(const struct pmemset_source *src,
		struct pmem2_source **pmem2_src)
{
	LOG(3, "src type %d", src->type);
	PMEMSET_ERR_CLR();

	if (src->type != PMEMSET_SOURCE_PMEM2) {
		ERR("pmem2_source is not set");
		return PMEMSET_E_INVALID_PMEM2_SOURCE;
	}

	*pmem2_src = src->value.pmem2_src;

	return 0;
}

/*
 * pmemset_source_get_pmem2_map_from_file -- get pmem2_map from
 * provided source (file type)
 */
int
pmemset_source_get_pmem2_map_from_file(const struct pmemset_source *src,
		struct pmem2_config *cfg, struct pmem2_map **map)
{
	LOG(3, "src type %d", src->type);

	if (src->type != PMEMSET_SOURCE_PATH) {
		ERR("invalid pmemset source type");
		return PMEMSET_E_INVALID_SOURCE_TYPE;
	}

	char *source_path;
	int ret = pmemset_source_get_filepath(src, &source_path);
	if (ret)
		return ret;

	int fd;
	if ((fd = os_open(source_path, O_RDWR)) < 0) {
		ERR("cannot open file from the source %s",
			source_path);
		return PMEMSET_E_ERRNO;
	}
	struct pmem2_source *pmem2_src;
	ret = pmem2_source_from_fd(&pmem2_src, fd);
	if (ret) {
		ERR("cannot get source from pmem2 using fd %d", fd);
		ret = PMEMSET_E_INVALID_PMEM2_SOURCE;
		goto end;
	}

	struct pmem2_map *pmem2_map;
	ret = pmem2_map_new(&pmem2_map, cfg, pmem2_src);
	if (ret) {
		ERR("cannot create pmem2 mapping %d", ret);
		ret = PMEMSET_E_INVALID_PMEM2_MAP;
		goto map_err;
	}

	*map = pmem2_map;

map_err:
	pmem2_source_delete(&pmem2_src);
end:
	os_close(fd);
	return ret;
}

/*
 * pmemset_source_get_pmem2_map_from_src -- get pmem2_map from
 * provided source (pmem2)
 */
int
pmemset_source_get_pmem2_map_from_src(const struct pmemset_source *src,
		struct pmem2_config *cfg, struct pmem2_map **map)
{
	LOG(3, "src type %d", src->type);

	if (src->type != PMEMSET_SOURCE_PMEM2) {
		ERR("invalid pmemset source type");
		return PMEMSET_E_INVALID_SOURCE_TYPE;
	}

	struct pmem2_source *pmem2_src;
	int ret = pmemset_source_get_pmem2_source(src, &pmem2_src);
	if (ret)
		return ret;

	struct pmem2_map *pmem2_map;
	ret = pmem2_map_new(&pmem2_map, cfg, pmem2_src);
	if (ret) {
		ERR("cannot create pmem2 mapping %d", ret);
		ret = PMEMSET_E_INVALID_PMEM2_MAP;
		goto end;
	}

	*map = pmem2_map;

end:
	return ret;
}

/*
 * pmemset_source_get_type -- get the type of provided source
 */
enum pmemset_source_type
pmemset_source_get_type(const struct pmemset_source *src)
{
	LOG(3, "src %p", src);

	return src->type;
}
