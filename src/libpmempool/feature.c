// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2023, Intel Corporation */

/*
 * feature.c -- implementation of pmempool_feature_(enable|disable|query)()
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include "libpmempool.h"
#include "util_pmem.h"
#include "pool_hdr.h"
#include "pool.h"

#define RW	0
#define RDONLY	1

#define FEATURE_INCOMPAT(X) \
	(features_t)FEAT_INCOMPAT(X)

static const features_t f_singlehdr = FEAT_INCOMPAT(SINGLEHDR);
static const features_t f_cksum_2k = FEAT_INCOMPAT(CKSUM_2K);
static const features_t f_sds = FEAT_INCOMPAT(SDS);
static const features_t f_chkbb = FEAT_COMPAT(CHECK_BAD_BLOCKS);

#define FEAT_INVALID \
	{UINT32_MAX, UINT32_MAX, UINT32_MAX};

static const features_t f_invalid = FEAT_INVALID;

#define FEATURE_MAXPRINT ((size_t)1024)

/*
 * buff_concat -- (internal) concat formatted string to string buffer
 */
static int
buff_concat(char *buff, size_t *pos, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	const size_t size = FEATURE_MAXPRINT - *pos - 1;
	int ret = vsnprintf(buff + *pos, size, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		ERR("vsprintf");
		return ret;
	}

	if ((size_t)ret >= size) {
		ERR("buffer truncated %d >= %zu", ret, size);
		return -1;
	}

	*pos += (size_t)ret;
	return 0;
}

/*
 * buff_concat_features -- (internal) concat features string to string buffer
 */
static int
buff_concat_features(char *buff, size_t *pos, features_t f)
{
	return buff_concat(buff, pos,
			"{compat 0x%x, incompat 0x%x, ro_compat 0x%x}",
			f.compat, f.incompat, f.ro_compat);
}

/*
 * poolset_close -- (internal) close pool set
 */
static void
poolset_close(struct pool_set *set)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		for (unsigned p = 0; p < rep->nparts; ++p) {
			util_unmap_hdr(PART(rep, p));
		}
	}

	util_poolset_close(set, DO_NOT_DELETE_PARTS);
}

/*
 * features_check -- (internal) check if features are correct
 */
static int
features_check(features_t *f, struct pool_hdr *hdrp)
{
	static char msg[FEATURE_MAXPRINT];

	struct pool_hdr hdr;
	memcpy(&hdr, hdrp, sizeof(hdr));
	util_convert2h_hdr_nocheck(&hdr);

	/* (f != f_invlaid) <=> features is set */
	if (!util_feature_cmp(*f, f_invalid)) {
		/* features from current and previous headers have to match */
		if (!util_feature_cmp(*f, hdr.features)) {
			size_t pos = 0;
			if (buff_concat_features(msg, &pos, hdr.features))
				goto err;
			if (buff_concat(msg, &pos, "%s", " != "))
				goto err;
			if (buff_concat_features(msg, &pos, *f))
				goto err;
			ERR("features mismatch detected: %s", msg);
			return -1;
		} else {
			return 0;
		}
	}

	features_t unknown = util_get_unknown_features(
			hdr.features, (features_t)POOL_FEAT_VALID);

	/* all features are known */
	if (util_feature_is_zero(unknown)) {
		memcpy(f, &hdr.features, sizeof(*f));
		return 0;
	}

	/* unknown features detected - print error message */
	size_t pos = 0;
	if (buff_concat_features(msg, &pos, unknown))
		goto err;
	ERR("invalid features detected: %s", msg);
err:
	return -1;
}

/*
 * get_pool_open_flags -- (internal) generate pool open flags
 */
static inline unsigned
get_pool_open_flags(struct pool_set *set, int rdonly)
{
	unsigned flags = 0;
	if (rdonly == RDONLY && !util_pool_has_device_dax(set))
		flags = POOL_OPEN_COW;
	flags |= POOL_OPEN_IGNORE_BAD_BLOCKS;
	return flags;
}

/*
 * get_mmap_flags -- (internal) generate mmap flags
 */
static inline int
get_mmap_flags(struct pool_set_part *part, int rdonly)
{
	if (part->is_dev_dax)
		return MAP_SHARED;
	else
		return rdonly ? MAP_PRIVATE : MAP_SHARED;
}

/*
 * poolset_open -- (internal) open pool set
 */
static struct pool_set *
poolset_open(const char *path, int rdonly)
{
	struct pool_set *set;
	features_t f = FEAT_INVALID;

	/* read poolset */
	int ret = util_poolset_create_set(&set, path, 0, 0, true);
	if (ret < 0) {
		ERR("cannot open pool set -- '%s'", path);
		goto err_poolset;
	}

	/* open a memory pool */
	unsigned flags = get_pool_open_flags(set, rdonly);
	if (util_pool_open_nocheck(set, flags)) {
		set = NULL;
		goto err_open;
	}

	/* map all headers and check features */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);

		for (unsigned p = 0; p < rep->nparts; ++p) {
			struct pool_set_part *part = PART(rep, p);
			int mmap_flags = get_mmap_flags(part, rdonly);
			if (util_map_hdr(part, mmap_flags, rdonly)) {
				part->hdr = NULL;
				goto err_map_hdr;
			}

			if (features_check(&f, HDR(rep, p))) {
				ERR(
					"invalid features - replica #%d part #%d",
					r, p);
				goto err_open;
			}
		}
	}
	return set;

err_map_hdr:
	/* unmap all headers */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		for (unsigned p = 0; p < rep->nparts; ++p) {
			util_unmap_hdr(PART(rep, p));
		}
	}
err_open:
	/* close the memory pool and release pool set structure */
	if (set)
		util_poolset_close(set, DO_NOT_DELETE_PARTS);
err_poolset:
	return NULL;
}

/*
 * get_hdr -- (internal) read header in host byte order
 */
static struct pool_hdr *
get_hdr(struct pool_set *set, unsigned rep, unsigned part)
{
	static struct pool_hdr hdr;

	/* copy header */
	struct pool_hdr *hdrp = HDR(REP(set, rep), part);
	memcpy(&hdr, hdrp, sizeof(hdr));

	/* convert to host byte order and return */
	util_convert2h_hdr_nocheck(&hdr);
	return &hdr;
}

/*
 * set_hdr -- (internal) convert header to little-endian, checksum and write
 */
static void
set_hdr(struct pool_set *set, unsigned rep, unsigned part, struct pool_hdr *src)
{
	/* convert to little-endian and set new checksum */
	const size_t skip_off = POOL_HDR_CSUM_END_OFF(src);
	util_convert2le_hdr(src);
	util_checksum(src, sizeof(*src), &src->checksum, 1, skip_off);

	/* write header */
	struct pool_replica *replica = REP(set, rep);
	struct pool_hdr *dst = HDR(replica, part);
	memcpy(dst, src, sizeof(*src));
	util_persist_auto(PART(replica, part)->is_dev_dax, dst, sizeof(*src));
}

typedef enum {
	DISABLED,
	ENABLED
} fstate_t;

#define FEATURE_IS_ENABLED_STR	"feature already enabled: %s"
#define FEATURE_IS_DISABLED_STR	"feature already disabled: %s"

/*
 * require_feature_is -- (internal) check if required feature is enabled
 * (or disabled)
 */
static int
require_feature_is(struct pool_set *set, features_t feature, fstate_t req_state)
{
	struct pool_hdr *hdrp = get_hdr((set), 0, 0);
	fstate_t state = util_feature_is_set(hdrp->features, feature)
			? ENABLED : DISABLED;
	if (state == req_state)
		return 1;

	const char *msg = (state == ENABLED)
			? FEATURE_IS_ENABLED_STR : FEATURE_IS_DISABLED_STR;
	LOG(3, msg, util_feature2str(feature, NULL));
	return 0;
}

#define FEATURE_IS_NOT_ENABLED_PRIOR_STR	"enable %s prior to %s %s"
#define FEATURE_IS_NOT_DISABLED_PRIOR_STR	"disable %s prior to %s %s"

/*
 * require_other_feature_is -- (internal) check if other feature is enabled
 * (or disabled) in case the other feature has to be enabled (or disabled)
 * prior to the main one
 */
static int
require_other_feature_is(struct pool_set *set, features_t other,
		fstate_t req_state, features_t feature, const char *cause)
{
	struct pool_hdr *hdrp = get_hdr((set), 0, 0);
	fstate_t state = util_feature_is_set(hdrp->features, other)
			? ENABLED : DISABLED;
	if (state == req_state)
		return 1;

	const char *msg = (req_state == ENABLED)
			? FEATURE_IS_NOT_ENABLED_PRIOR_STR
			: FEATURE_IS_NOT_DISABLED_PRIOR_STR;
	ERR(msg, util_feature2str(other, NULL),
			cause, util_feature2str(feature, NULL));
	return 0;
}

/*
 * feature_set -- (internal) enable (or disable) feature
 */
static void
feature_set(struct pool_set *set, features_t feature, int value)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		for (unsigned p = 0; p < REP(set, r)->nparts; ++p) {
			struct pool_hdr *hdrp = get_hdr(set, r, p);
			if (value == ENABLED)
				util_feature_enable(&hdrp->features, feature);
			else
				util_feature_disable(&hdrp->features, feature);
			set_hdr(set, r, p, hdrp);
		}
	}
}

/*
 * query_feature -- (internal) query feature value
 */
static int
query_feature(const char *path, features_t feature)
{
	struct pool_set *set = poolset_open(path, RDONLY);
	if (!set)
		goto err_open;

	struct pool_hdr *hdrp = get_hdr(set, 0, 0);
	const int query = util_feature_is_set(hdrp->features, feature);

	poolset_close(set);

	return query;

err_open:
	return -1;
}

/*
 * unsupported_feature -- (internal) report unsupported feature
 */
static inline int
unsupported_feature(features_t feature)
{
	ERR("unsupported feature: %s", util_feature2str(feature, NULL));
	errno = EINVAL;
	return -1;
}

/*
 * enable_singlehdr -- (internal) enable POOL_FEAT_SINGLEHDR
 */
static int
enable_singlehdr(const char *path)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(path);

	return unsupported_feature(f_singlehdr);
}

/*
 * disable_singlehdr -- (internal) disable POOL_FEAT_SINGLEHDR
 */
static int
disable_singlehdr(const char *path)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(path);

	return unsupported_feature(f_singlehdr);
}

/*
 * query_singlehdr -- (internal) query POOL_FEAT_SINGLEHDR
 */
static int
query_singlehdr(const char *path)
{
	return query_feature(path, f_singlehdr);
}

/*
 * enable_checksum_2k -- (internal) enable POOL_FEAT_CKSUM_2K
 */
static int
enable_checksum_2k(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;
	if (require_feature_is(set, f_cksum_2k, DISABLED))
		feature_set(set, f_cksum_2k, ENABLED);

	poolset_close(set);
	return 0;
}

/*
 * disable_checksum_2k -- (internal) disable POOL_FEAT_CKSUM_2K
 */
static int
disable_checksum_2k(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;

	int ret = 0;
	if (!require_feature_is(set, f_cksum_2k, ENABLED))
		goto exit;

	/* check if POOL_FEAT_SDS is disabled */
	if (!require_other_feature_is(set, f_sds, DISABLED,
			f_cksum_2k, "disabling")) {
		ret = -1;
		goto exit;
	}

	feature_set(set, f_cksum_2k, DISABLED);
exit:
	poolset_close(set);
	return ret;
}

/*
 * query_checksum_2k -- (internal) query POOL_FEAT_CKSUM_2K
 */
static int
query_checksum_2k(const char *path)
{
	return query_feature(path, f_cksum_2k);
}

/*
 * enable_shutdown_state -- (internal) enable POOL_FEAT_SDS
 */
static int
enable_shutdown_state(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;

	int ret = 0;
	if (!require_feature_is(set, f_sds, DISABLED))
		goto exit;

	/* check if POOL_FEAT_CKSUM_2K is enabled */
	if (!require_other_feature_is(set, f_cksum_2k, ENABLED,
			f_sds, "enabling")) {
		ret = -1;
		goto exit;
	}

	feature_set(set, f_sds, ENABLED);

exit:
	poolset_close(set);
	return ret;
}

/*
 * reset_shutdown_state -- zero all shutdown structures
 */
static void
reset_shutdown_state(struct pool_set *set)
{
	for (unsigned rep = 0; rep < set->nreplicas; ++rep) {
		for (unsigned part = 0; part < REP(set, rep)->nparts; ++part) {
			struct pool_hdr *hdrp = HDR(REP(set, rep), part);
			shutdown_state_init(&hdrp->sds, REP(set, rep));
		}
	}
}

/*
 * disable_shutdown_state -- (internal) disable POOL_FEAT_SDS
 */
static int
disable_shutdown_state(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;

	if (require_feature_is(set, f_sds, ENABLED)) {
		feature_set(set, f_sds, DISABLED);
		reset_shutdown_state(set);
	}

	poolset_close(set);
	return 0;
}

/*
 * query_shutdown_state -- (internal) query POOL_FEAT_SDS
 */
static int
query_shutdown_state(const char *path)
{
	return query_feature(path, f_sds);
}

/*
 * enable_badblocks_checking -- (internal) enable POOL_FEAT_CHECK_BAD_BLOCKS
 */
static int
enable_badblocks_checking(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;

	if (require_feature_is(set, f_chkbb, DISABLED))
		feature_set(set, f_chkbb, ENABLED);

	poolset_close(set);

	return 0;
}

/*
 * disable_badblocks_checking -- (internal) disable POOL_FEAT_CHECK_BAD_BLOCKS
 */
static int
disable_badblocks_checking(const char *path)
{
	struct pool_set *set = poolset_open(path, RW);
	if (!set)
		return -1;

	int ret = 0;
	if (!require_feature_is(set, f_chkbb, ENABLED))
		goto exit;

	feature_set(set, f_chkbb, DISABLED);
exit:
	poolset_close(set);

	return ret;
}

/*
 * query_badblocks_checking -- (internal) query POOL_FEAT_CHECK_BAD_BLOCKS
 */
static int
query_badblocks_checking(const char *path)
{
	return query_feature(path, f_chkbb);
}

struct feature_funcs {
	int (*enable)(const char *);
	int (*disable)(const char *);
	int (*query)(const char *);
};

static struct feature_funcs features[] = {
		{
			.enable = enable_singlehdr,
			.disable = disable_singlehdr,
			.query = query_singlehdr
		},
		{
			.enable = enable_checksum_2k,
			.disable = disable_checksum_2k,
			.query = query_checksum_2k
		},
		{
			.enable = enable_shutdown_state,
			.disable = disable_shutdown_state,
			.query = query_shutdown_state
		},
		{
			.enable = enable_badblocks_checking,
			.disable = disable_badblocks_checking,
			.query = query_badblocks_checking
		},
};

#define FEATURE_FUNCS_MAX ARRAY_SIZE(features)

/*
 * are_flags_valid -- (internal) check if flags are valid
 */
static inline int
are_flags_valid(unsigned flags)
{
	if (flags != 0) {
		ERR("invalid flags: 0x%x", flags);
		errno = EINVAL;
		return 0;
	}
	return 1;
}

/*
 * is_feature_valid -- (internal) check if feature is valid
 */
static inline int
is_feature_valid(uint32_t feature)
{
	if (feature >= FEATURE_FUNCS_MAX) {
		ERR("invalid feature: 0x%x", feature);
		errno = EINVAL;
		return 0;
	}
	return 1;
}

/*
 * pmempool_feature_enableU -- enable pool set feature
 */
static inline
int
pmempool_feature_enableU(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	LOG(3, "path %s feature %x flags %x", path, feature, flags);
	if (!is_feature_valid(feature))
		return -1;
	if (!are_flags_valid(flags))
		return -1;
	return features[feature].enable(path);
}

/*
 * pmempool_feature_disableU -- disable pool set feature
 */
static inline
int
pmempool_feature_disableU(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	LOG(3, "path %s feature %x flags %x", path, feature, flags);
	if (!is_feature_valid(feature))
		return -1;
	if (!are_flags_valid(flags))
		return -1;
	return features[feature].disable(path);
}

/*
 * pmempool_feature_queryU -- query pool set feature
 */
static inline
int
pmempool_feature_queryU(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	LOG(3, "path %s feature %x flags %x", path, feature, flags);

	/*
	 * XXX: Windows does not allow function call in a constant expressions
	 */
#define CHECK_INCOMPAT_MAPPING(FEAT, ENUM) \
	COMPILE_ERROR_ON( \
		util_feature2pmempool_feature(FEATURE_INCOMPAT(FEAT)) != ENUM)

	CHECK_INCOMPAT_MAPPING(SINGLEHDR, PMEMPOOL_FEAT_SINGLEHDR);
	CHECK_INCOMPAT_MAPPING(CKSUM_2K, PMEMPOOL_FEAT_CKSUM_2K);
	CHECK_INCOMPAT_MAPPING(SDS, PMEMPOOL_FEAT_SHUTDOWN_STATE);

#undef CHECK_INCOMPAT_MAPPING

	if (!is_feature_valid(feature))
		return -1;
	if (!are_flags_valid(flags))
		return -1;
	return features[feature].query(path);
}

/*
 * pmempool_feature_enable -- enable pool set feature
 */
int
pmempool_feature_enable(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	return pmempool_feature_enableU(path, feature, flags);
}

/*
 * pmempool_feature_disable -- disable pool set feature
 */
int
pmempool_feature_disable(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	return pmempool_feature_disableU(path, feature, flags);
}

/*
 * pmempool_feature_query -- query pool set feature
 */
int
pmempool_feature_query(const char *path, enum pmempool_feature feature,
	unsigned flags)
{
	return pmempool_feature_queryU(path, feature, flags);
}
