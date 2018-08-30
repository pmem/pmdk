/*
 * Copyright 2018, Intel Corporation
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
 * feature.c -- implementation of pmempool_feature_(enable|disable|query)()
 */

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>

#include "libpmempool.h"
#include "pool.h"

#define RW	0
#define RDONLY	1

/*
 * poolset_close -- (internal) close pool set
 */
static void
poolset_close(struct pool_set *set)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		if (rep->remote) {
			continue;
		} else {
			for (unsigned p = 0; p < rep->nparts; ++p) {
				util_unmap_hdr(PART(rep, p));
			}
		}
	}
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
}

/*
 * incompat_features_check -- (internal) check if incompat features are correct
 */
static int
incompat_features_check(uint32_t *incompat_features, struct pool_hdr *hdrp)
{
	struct pool_hdr hdr;
	memcpy(&hdr, hdrp, sizeof(hdr));
	util_convert2h_hdr_nocheck(&hdr);

	if (*incompat_features == UINT32_MAX) {
		uint32_t unknown = (uint32_t)util_get_not_masked_bits(
				hdr.incompat_features, POOL_FEAT_VALID);
		if (unknown) {
			fprintf(stderr, "invalid features detected: 0x%x\n",
					unknown);
			return -1;
		} else {
			*incompat_features = hdr.incompat_features;
		}
	} else {
		if (*incompat_features != hdr.incompat_features) {
			fprintf(stderr, "features mismatch detected: "
					"0x%x != 0x%x\n",
					hdr.incompat_features,
					*incompat_features);
			return -1;
		}
	}
	return 0;
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
	uint32_t incompat_features = UINT32_MAX;

	/* read poolset */
	int ret = util_poolset_create_set(&set, path, 0, 0, true);
	if (ret < 0) {
		ERR("cannot open pool set -- '%s'", path);
		goto err_poolset;
	}

	/* open a memory pool */
	unsigned flags = (rdonly ? POOL_OPEN_COW : 0) |
			POOL_OPEN_IGNORE_BAD_BLOCKS;
	if (util_pool_open_nocheck(set, flags))
		goto err_open;

	/* map all headers and check incompat features */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		if (rep->remote) {
			ERR("poolsets with remote replicas are not supported");
			goto err_open;
		} else {
			for (unsigned p = 0; p < rep->nparts; ++p) {
				struct pool_set_part *part = PART(rep, p);
				int mmap_flags = get_mmap_flags(part, rdonly);
				if (util_map_hdr(part, mmap_flags, rdonly)) {
					part->hdr = NULL;
					goto err_map_hdr;
				}

				if (incompat_features_check(&incompat_features,
						HDR(rep, p))) {
					ERR("invalid incompat features - "
							"replica #%d part #%d",
							r, p);
					goto err_open;
				}
			}
		}
	}
	return set;

err_map_hdr:
	/* unmap all headers */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		if (rep->remote) {
			continue;
		} else {
			for (unsigned p = 0; p < rep->nparts; ++p) {
				util_unmap_hdr(PART(rep, p));
			}
		}
	}
err_open:
	/* close the memory pool and release pool set structure */
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
	struct pool_hdr *dst = HDR(REP(set, rep), part);
	memcpy(dst, src, sizeof(*src));
}

#define FEATURE_IS_ENABLED(set, feature) \
	(get_hdr((set), 0, 0)->incompat_features & (feature))

#define FEATURE_IS_DISABLED(set, feature) \
	(!(get_hdr((set), 0, 0)->incompat_features & (feature)))

#define FEATURE_IS_ENABLED_STR	"feature already enabled: %s"
#define FEATURE_IS_DISABLED_STR	"feature already disabled: %s"

#define FEATURE_IS_NOT_ENABLED_STR	FEATURE_IS_DISABLED_STR
#define FEATURE_IS_NOT_DISABLED_STR	FEATURE_IS_ENABLED_STR

/*
 * require_feature_is_(ENABLED|DISABLED) -- (internal) check if required
 * feature is enabled (or disabled)
 */
#define REQ_FEATURE_IS_X_FUNC(X) \
static int \
require_feature_is_##X(struct pool_set *set, uint32_t feature) \
{ \
	if (!FEATURE_IS_##X(set, feature)) { \
		LOG(3, FEATURE_IS_NOT_##X##_STR, \
				out_feature2str(feature, NULL)); \
		return 0; \
	} \
	return 1; \
}

REQ_FEATURE_IS_X_FUNC(ENABLED)
REQ_FEATURE_IS_X_FUNC(DISABLED)

#define FEATURE_IS_NOT_ENABLED_PRIOR_STR	"enable %s prior to %s %s\n"
#define FEATURE_IS_NOT_DISABLED_PRIOR_STR	"disable %s prior to %s %s\n"

/*
 * require_other_feature_is_(ENABLED|DISABLED) -- (internal) check if other
 * feature is enabled (or disabled) in case the other feature has to be enabled
 * (or disabled) prior to the main one
 */
#define REQ_OTHER_FEATURE_IS_X_FUNC(X) \
static int \
require_other_feature_is_##X(struct pool_set *set, \
		uint32_t main, uint32_t other, const char *op) \
{ \
	if (!FEATURE_IS_##X(set, other)) { \
		const char *main_str = out_feature2str(main, NULL); \
		const char *other_str = out_feature2str(other, NULL); \
		fprintf(stderr, FEATURE_IS_NOT_##X##_PRIOR_STR, \
				other_str, op, main_str); \
		return 0; \
	} \
	return 1; \
}

REQ_OTHER_FEATURE_IS_X_FUNC(ENABLED)
REQ_OTHER_FEATURE_IS_X_FUNC(DISABLED)

#define FEATURE_ENABLE(incompat_features, feature) \
	(incompat_features) |= (feature)

#define FEATURE_DISABLE(incompat_features, feature) \
	(incompat_features) &= ~(feature)

/*
 * (ENABLE|DISABLE)_feature -- (internal) enable (or disable) feature in all
 * pool set headers
 */
#define X_FEATURE_FUNC(X) \
static void \
feature_##X(struct pool_set *set, uint32_t feature) \
{ \
	for (unsigned r = 0; r < set->nreplicas; ++r) { \
		for (unsigned p = 0; p < REP(set, r)->nparts; ++p) { \
			struct pool_hdr *hdrp = get_hdr(set, r, p); \
			FEATURE_##X(hdrp->incompat_features, feature); \
			set_hdr(set, r, p, hdrp); \
		} \
	} \
}

X_FEATURE_FUNC(ENABLE)
X_FEATURE_FUNC(DISABLE)

/*
 * query_feature -- (internal) query feature value
 */
static int
query_feature(const char *path, uint32_t feature)
{
	struct pool_set *set = poolset_open(path, RDONLY);
	if (!set)
		goto err_open;

	struct pool_hdr *hdrp = get_hdr(set, 0, 0);
	const int query = (hdrp->incompat_features & feature) ? 1 : 0;

	poolset_close(set);

	return query;

err_open:
	return -1;
}

/*
 * unsupported_feature -- (internal) report unsupported feature
 */
static inline int
unsupported_feature(uint32_t feature)
{
	fprintf(stderr, "unsupported feature: %s\n",
			out_feature2str(feature, NULL));
	errno = EINVAL;
	return -1;
}

/*
 * enable_singlehdr -- (internal) enable POOL_FEAT_SINGLEHDR
 */
static int
enable_singlehdr(const char *path)
{
	return unsupported_feature(POOL_FEAT_SINGLEHDR);
}

/*
 * disable_singlehdr -- (internal) disable POOL_FEAT_SINGLEHDR
 */
static int
disable_singlehdr(const char *path)
{
	return unsupported_feature(POOL_FEAT_SINGLEHDR);
}

/*
 * query_singlehdr -- (internal) query POOL_FEAT_SINGLEHDR
 */
static int
query_singlehdr(const char *path)
{
	return query_feature(path, POOL_FEAT_SINGLEHDR);
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

	if (require_feature_is_DISABLED(set, POOL_FEAT_CKSUM_2K))
		feature_ENABLE(set, POOL_FEAT_CKSUM_2K);

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
	if (!require_feature_is_ENABLED(set, POOL_FEAT_CKSUM_2K))
		goto exit;

	/* disable POOL_FEAT_SDS prior to disabling POOL_FEAT_CKSUM_2K */
	if (!require_other_feature_is_DISABLED(set,
			POOL_FEAT_CKSUM_2K, POOL_FEAT_SDS, "disabling")) {
		ret = -1;
		goto exit;
	}

	feature_DISABLE(set, POOL_FEAT_CKSUM_2K);
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
	return query_feature(path, POOL_FEAT_CKSUM_2K);
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
	if (!require_feature_is_DISABLED(set, POOL_FEAT_SDS))
		goto exit;

	/* enable POOL_FEAT_CKSUM_2K prior to enabling POOL_FEAT_SDS */
	if (!require_other_feature_is_ENABLED(set,
			POOL_FEAT_SDS, POOL_FEAT_CKSUM_2K, "enabling")) {
		ret = -1;
		goto exit;
	}

	feature_ENABLE(set, POOL_FEAT_SDS);

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

	if (require_feature_is_ENABLED(set, POOL_FEAT_SDS)) {
		feature_DISABLE(set, POOL_FEAT_SDS);
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
	return query_feature(path, POOL_FEAT_SDS);
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
};

#define FEATURE_FUNCS_MAX ARRAY_SIZE(features)

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
#ifndef _WIN32
static inline
#endif
int
pmempool_feature_enableU(const char *path, enum pmempool_feature feature)
{
	LOG(3, "path %s, feature %x", path, feature);
	if (!is_feature_valid(feature))
		return -1;
	return features[feature].enable(path);
}

/*
 * pmempool_feature_disableU -- disable pool set feature
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_feature_disableU(const char *path, enum pmempool_feature feature)
{
	LOG(3, "path %s, feature %x", path, feature);
	if (!is_feature_valid(feature))
		return -1;
	return features[feature].disable(path);
}

/*
 * pmempool_feature_queryU -- query pool set feature
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_feature_queryU(const char *path, enum pmempool_feature feature)
{
	LOG(3, "path %s, feature %x", path, feature);
	if (!is_feature_valid(feature))
		return -1;
	return features[feature].query(path);
}

#ifndef _WIN32
/*
 * pmempool_feature_enable -- enable pool set feature
 */
int
pmempool_feature_enable(const char *path, enum pmempool_feature feature)
{
	return pmempool_feature_enableU(path, feature);
}
#else
/*
 * pmempool_feature_enableW -- enable pool set feature as widechar
 */
int
pmempool_feature_enableW(const wchar_t *path, enum pmempool_feature feature)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL) {
		ERR("Invalid poolest/pool file path.");
		return -1;
	}

	int ret = pmempool_feature_enableU(upath, feature);

	util_free_UTF8(upath);
	return ret;
}
#endif

#ifndef _WIN32
/*
 * pmempool_feature_disable -- disable pool set feature
 */
int
pmempool_feature_disable(const char *path, enum pmempool_feature feature)
{
	return pmempool_feature_disableU(path, feature);
}
#else
/*
 * pmempool_feature_disableW -- disable pool set feature as widechar
 */
int
pmempool_feature_disableW(const wchar_t *path, enum pmempool_feature feature)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL) {
		ERR("Invalid poolest/pool file path.");
		return -1;
	}

	int ret = pmempool_feature_disableU(upath, feature);

	util_free_UTF8(upath);
	return ret;
}
#endif

#ifndef _WIN32
/*
 * pmempool_feature_query -- query pool set feature
 */
int
pmempool_feature_query(const char *path, enum pmempool_feature feature)
{
	return pmempool_feature_queryU(path, feature);
}
#else
/*
 * pmempool_feature_queryW -- query pool set feature as widechar
 */
int
pmempool_feature_queryW(const wchar_t *path, enum pmempool_feature feature)
{
	char *upath = util_toUTF8(path);
	if (upath == NULL) {
		ERR("Invalid poolest/pool file path.");
		return -1;
	}

	int ret = pmempool_feature_queryU(upath, feature);

	util_free_UTF8(upath);
	return ret;
}
#endif
