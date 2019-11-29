/*
 * Copyright 2014-2019, Intel Corporation
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
 * pool_hdr.c -- pool header utilities
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>

#include "out.h"
#include "pool_hdr.h"

/* Determine ISA for which PMDK is currently compiled */
#if defined(__x86_64) || defined(_M_X64)
/* x86 -- 64 bit */
#define PMDK_MACHINE PMDK_MACHINE_X86_64
#define PMDK_MACHINE_CLASS PMDK_MACHINE_CLASS_64

#elif defined(__aarch64__)
/* 64 bit ARM not supported yet */
#define PMDK_MACHINE PMDK_MACHINE_AARCH64
#define PMDK_MACHINE_CLASS PMDK_MACHINE_CLASS_64

#elif defined(__PPC64__)
#define PMDK_MACHINE PMDK_MACHINE_PPC64
#define PMDK_MACHINE_CLASS PMDK_MACHINE_CLASS_64

#else
/* add appropriate definitions here when porting PMDK to another ISA */
#error unable to recognize ISA at compile time

#endif

/*
 * arch_machine -- (internal) determine endianness
 */
static uint8_t
arch_data(void)
{
	uint16_t word = (PMDK_DATA_BE << 8) + PMDK_DATA_LE;
	return ((uint8_t *)&word)[0];
}

/*
 * util_get_arch_flags -- get architecture identification flags
 */
void
util_get_arch_flags(struct arch_flags *arch_flags)
{
	memset(arch_flags, 0, sizeof(*arch_flags));
	arch_flags->machine = PMDK_MACHINE;
	arch_flags->machine_class = PMDK_MACHINE_CLASS;
	arch_flags->data = arch_data();
	arch_flags->alignment_desc = alignment_desc();
}

/*
 * util_convert2le_hdr -- convert pool_hdr into little-endian byte order
 */
void
util_convert2le_hdr(struct pool_hdr *hdrp)
{
	hdrp->major = htole32(hdrp->major);
	hdrp->features.compat = htole32(hdrp->features.compat);
	hdrp->features.incompat = htole32(hdrp->features.incompat);
	hdrp->features.ro_compat = htole32(hdrp->features.ro_compat);
	hdrp->arch_flags.alignment_desc =
		htole64(hdrp->arch_flags.alignment_desc);
	hdrp->arch_flags.machine = htole16(hdrp->arch_flags.machine);
	hdrp->crtime = htole64(hdrp->crtime);
	hdrp->checksum = htole64(hdrp->checksum);
}

/*
 * util_convert2h_hdr_nocheck -- convert pool_hdr into host byte order
 */
void
util_convert2h_hdr_nocheck(struct pool_hdr *hdrp)
{
	hdrp->major = le32toh(hdrp->major);
	hdrp->features.compat = le32toh(hdrp->features.compat);
	hdrp->features.incompat = le32toh(hdrp->features.incompat);
	hdrp->features.ro_compat = le32toh(hdrp->features.ro_compat);
	hdrp->crtime = le64toh(hdrp->crtime);
	hdrp->arch_flags.machine = le16toh(hdrp->arch_flags.machine);
	hdrp->arch_flags.alignment_desc =
		le64toh(hdrp->arch_flags.alignment_desc);
	hdrp->checksum = le64toh(hdrp->checksum);
}

/*
 * util_arch_flags_check -- validates arch_flags
 */
int
util_check_arch_flags(const struct arch_flags *arch_flags)
{
	struct arch_flags cur_af;
	int ret = 0;

	util_get_arch_flags(&cur_af);

	if (!util_is_zeroed(&arch_flags->reserved,
				sizeof(arch_flags->reserved))) {
		ERR("invalid reserved values");
		ret = -1;
	}

	if (arch_flags->machine != cur_af.machine) {
		ERR("invalid machine value");
		ret = -1;
	}

	if (arch_flags->data != cur_af.data) {
		ERR("invalid data value");
		ret = -1;
	}

	if (arch_flags->machine_class != cur_af.machine_class) {
		ERR("invalid machine_class value");
		ret = -1;
	}

	if (arch_flags->alignment_desc != cur_af.alignment_desc) {
		ERR("invalid alignment_desc value");
		ret = -1;
	}

	return ret;
}

/*
 * util_get_unknown_features -- filter out unknown features flags
 */
features_t
util_get_unknown_features(features_t features, features_t known)
{
	features_t unknown;
	unknown.compat = util_get_not_masked_bits(
			features.compat, known.compat);
	unknown.incompat = util_get_not_masked_bits(
			features.incompat, known.incompat);
	unknown.ro_compat = util_get_not_masked_bits(
			features.ro_compat, known.ro_compat);
	return unknown;
}

/*
 * util_feature_check -- check features masks
 */
int
util_feature_check(struct pool_hdr *hdrp, features_t known)
{
	LOG(3, "hdrp %p features {incompat %#x ro_compat %#x compat %#x}",
			hdrp,
			known.incompat, known.ro_compat, known.compat);

	features_t unknown = util_get_unknown_features(hdrp->features, known);

	/* check incompatible ("must support") features */
	if (unknown.incompat) {
		ERR("unsafe to continue due to unknown incompat "\
				"features: %#x", unknown.incompat);
		errno = EINVAL;
		return -1;
	}

	/* check RO-compatible features (force RO if unsupported) */
	if (unknown.ro_compat) {
		ERR("switching to read-only mode due to unknown ro_compat "\
				"features: %#x", unknown.ro_compat);
		return 0;
	}

	/* check compatible ("may") features */
	if (unknown.compat) {
		LOG(3, "ignoring unknown compat features: %#x", unknown.compat);
	}

	return 1;
}

/*
 * util_feature_cmp -- compares features with reference
 *
 * returns 1 if features and reference match and 0 otherwise
 */
int
util_feature_cmp(features_t features, features_t ref)
{
	LOG(3, "features {incompat %#x ro_compat %#x compat %#x} "
			"ref {incompat %#x ro_compat %#x compat %#x}",
			features.incompat, features.ro_compat, features.compat,
			ref.incompat, ref.ro_compat, ref.compat);

	return features.compat == ref.compat &&
			features.incompat == ref.incompat &&
			features.ro_compat == ref.ro_compat;
}

/*
 * util_feature_is_zero -- check if features flags are zeroed
 *
 * returns 1 if features is zeroed and 0 otherwise
 */
int
util_feature_is_zero(features_t features)
{
	const uint32_t bits =
			features.compat | features.incompat |
			features.ro_compat;
	return bits ? 0 : 1;
}

/*
 * util_feature_is_set -- check if feature flag is set in features
 *
 * returns 1 if feature flag is set and 0 otherwise
 */
int
util_feature_is_set(features_t features, features_t flag)
{
	uint32_t bits = 0;
	bits |= features.compat & flag.compat;
	bits |= features.incompat & flag.incompat;
	bits |= features.ro_compat & flag.ro_compat;
	return bits ? 1 : 0;
}

/*
 * util_feature_enable -- enable feature
 */
void
util_feature_enable(features_t *features, features_t new_feature)
{
#define FEATURE_ENABLE(flags, X) \
	(flags) |= (X)

	FEATURE_ENABLE(features->compat, new_feature.compat);
	FEATURE_ENABLE(features->incompat, new_feature.incompat);
	FEATURE_ENABLE(features->ro_compat, new_feature.ro_compat);

#undef FEATURE_ENABLE
}

/*
 * util_feature_disable -- (internal) disable feature
 */
void
util_feature_disable(features_t *features, features_t old_feature)
{
#define FEATURE_DISABLE(flags, X) \
	(flags) &= ~(X)

	FEATURE_DISABLE(features->compat, old_feature.compat);
	FEATURE_DISABLE(features->incompat, old_feature.incompat);
	FEATURE_DISABLE(features->ro_compat, old_feature.ro_compat);

#undef FEATURE_DISABLE
}

static const features_t feature_2_pmempool_feature_map[] = {
	FEAT_INCOMPAT(SINGLEHDR),	/* PMEMPOOL_FEAT_SINGLEHDR */
	FEAT_INCOMPAT(CKSUM_2K),	/* PMEMPOOL_FEAT_CKSUM_2K */
	FEAT_INCOMPAT(SDS),		/* PMEMPOOL_FEAT_SHUTDOWN_STATE */
	FEAT_COMPAT(CHECK_BAD_BLOCKS),	/* PMEMPOOL_FEAT_CHECK_BAD_BLOCKS */
};

#define FEAT_2_PMEMPOOL_FEATURE_MAP_SIZE \
	ARRAY_SIZE(feature_2_pmempool_feature_map)

static const char *str_2_pmempool_feature_map[] = {
	"SINGLEHDR",
	"CKSUM_2K",
	"SHUTDOWN_STATE",
	"CHECK_BAD_BLOCKS",
};

#define PMEMPOOL_FEATURE_2_STR_MAP_SIZE ARRAY_SIZE(str_2_pmempool_feature_map)

/*
 * util_str2feature -- convert string to feat_flags value
 */
features_t
util_str2feature(const char *str)
{
	/* all features have to be named in incompat_features_str array */
	COMPILE_ERROR_ON(FEAT_2_PMEMPOOL_FEATURE_MAP_SIZE !=
			PMEMPOOL_FEATURE_2_STR_MAP_SIZE);

	for (uint32_t f = 0; f < PMEMPOOL_FEATURE_2_STR_MAP_SIZE; ++f) {
		if (strcmp(str, str_2_pmempool_feature_map[f]) == 0) {
			return feature_2_pmempool_feature_map[f];
		}
	}
	return features_zero;
}

/*
 * util_feature2pmempool_feature -- convert feature to pmempool_feature
 */
uint32_t
util_feature2pmempool_feature(features_t feat)
{
	for (uint32_t pf = 0; pf < FEAT_2_PMEMPOOL_FEATURE_MAP_SIZE; ++pf) {
		const features_t *record =
				&feature_2_pmempool_feature_map[pf];
		if (util_feature_cmp(feat, *record)) {
			return pf;
		}
	}
	return UINT32_MAX;
}

/*
 * util_str2pmempool_feature -- convert string to uint32_t enum pmempool_feature
 * equivalent
 */
uint32_t
util_str2pmempool_feature(const char *str)
{
	features_t fval = util_str2feature(str);
	if (util_feature_is_zero(fval))
		return UINT32_MAX;
	return util_feature2pmempool_feature(fval);
}

/*
 * util_feature2str -- convert uint32_t feature to string
 */
const char *
util_feature2str(features_t features, features_t *found)
{
	for (uint32_t i = 0; i < FEAT_2_PMEMPOOL_FEATURE_MAP_SIZE; ++i) {
		const features_t *record = &feature_2_pmempool_feature_map[i];
		if (util_feature_is_set(features, *record)) {
			if (found)
				memcpy(found, record, sizeof(features_t));
			return str_2_pmempool_feature_map[i];
		}
	}
	return NULL;
}
