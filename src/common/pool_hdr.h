/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2021, Intel Corporation */

/*
 * pool_hdr.h -- internal definitions for pool header module
 */

#ifndef PMDK_POOL_HDR_H
#define PMDK_POOL_HDR_H 1

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include "uuid.h"
#include "shutdown_state.h"
#include "util.h"
#include "page_size.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Number of bits per type in alignment descriptor
 */
#define ALIGNMENT_DESC_BITS		4

/*
 * architecture identification flags
 *
 * These flags allow to unambiguously determine the architecture
 * on which the pool was created.
 *
 * The alignment_desc field contains information about alignment
 * of the following basic types:
 * - char
 * - short
 * - int
 * - long
 * - long long
 * - size_t
 * - os_off_t
 * - float
 * - double
 * - long double
 * - void *
 *
 * The alignment of each type is computed as an offset of field
 * of specific type in the following structure:
 * struct {
 *	char byte;
 *	type field;
 * };
 *
 * The value is decremented by 1 and masked by 4 bits.
 * Multiple alignments are stored on consecutive 4 bits of each
 * type in the order specified above.
 *
 * The values used in the machine, and machine_class fields are in
 * principle independent of operating systems, and object formats.
 * In practice they happen to match constants used in ELF object headers.
 */
struct arch_flags {
	uint64_t alignment_desc;	/* alignment descriptor */
	uint8_t machine_class;		/* address size -- 64 bit or 32 bit */
	uint8_t data;			/* data encoding -- LE or BE */
	uint8_t reserved[4];
	uint16_t machine;		/* required architecture */
};

#define POOL_HDR_ARCH_LEN sizeof(struct arch_flags)

/* possible values of the machine class field in the above struct */
#define PMDK_MACHINE_CLASS_64 2 /* 64 bit pointers, 64 bit size_t */

/* possible values of the machine field in the above struct */
#define PMDK_MACHINE_X86_64 62
#define PMDK_MACHINE_AARCH64 183
#define PMDK_MACHINE_PPC64 21
#define PMDK_MACHINE_RISCV64 243
#define PMDK_MACHINE_LOONGARCH64 258

/* possible values of the data field in the above struct */
#define PMDK_DATA_LE 1 /* 2's complement, little endian */
#define PMDK_DATA_BE 2 /* 2's complement, big endian */

/*
 * features flags
 */
typedef struct {
	uint32_t compat;	/* mask: compatible "may" features */
	uint32_t incompat;	/* mask: "must support" features */
	uint32_t ro_compat;	/* mask: force RO if unsupported */
} features_t;

/*
 * header used at the beginning of all types of memory pools
 *
 * for pools build on persistent memory, the integer types
 * below are stored in little-endian byte order.
 */
#define POOL_HDR_SIG_LEN 8
#define POOL_HDR_UNUSED_SIZE 1904
#define POOL_HDR_UNUSED2_SIZE 1976
#define POOL_HDR_ALIGN_PAD (PMEM_PAGESIZE - 4096)
struct pool_hdr {
	char signature[POOL_HDR_SIG_LEN];
	uint32_t major;			/* format major version number */
	features_t features;		/* features flags */
	uuid_t poolset_uuid;		/* pool set UUID */
	uuid_t uuid;			/* UUID of this file */
	uuid_t prev_part_uuid;		/* prev part */
	uuid_t next_part_uuid;		/* next part */
	uuid_t prev_repl_uuid;		/* prev replica */
	uuid_t next_repl_uuid;		/* next replica */
	uint64_t crtime;		/* when created (seconds since epoch) */
	struct arch_flags arch_flags;	/* architecture identification flags */
	unsigned char unused[POOL_HDR_UNUSED_SIZE];	/* must be zero */
	/* not checksummed */
	unsigned char unused2[POOL_HDR_UNUSED2_SIZE];	/* must be zero */
	struct shutdown_state sds;	/* shutdown status */
	uint64_t checksum;		/* checksum of above fields */

#if PMEM_PAGESIZE > 4096 /* prevent zero size array */
	unsigned char align_pad[POOL_HDR_ALIGN_PAD];	/* alignment pad */
#endif
};

#define POOL_HDR_SIZE	(sizeof(struct pool_hdr))

#define POOL_DESC_SIZE PMEM_PAGESIZE

void util_convert2le_hdr(struct pool_hdr *hdrp);
void util_convert2h_hdr_nocheck(struct pool_hdr *hdrp);

void util_get_arch_flags(struct arch_flags *arch_flags);
int util_check_arch_flags(const struct arch_flags *arch_flags);

features_t util_get_unknown_features(features_t features, features_t known);
int util_feature_check(struct pool_hdr *hdrp, features_t features);
int util_feature_cmp(features_t features, features_t ref);
int util_feature_is_zero(features_t features);
int util_feature_is_set(features_t features, features_t flag);
void util_feature_enable(features_t *features, features_t new_feature);
void util_feature_disable(features_t *features, features_t new_feature);

const char *util_feature2str(features_t feature, features_t *found);
features_t util_str2feature(const char *str);
uint32_t util_str2pmempool_feature(const char *str);
uint32_t util_feature2pmempool_feature(features_t feat);

/*
 * set of macros for determining the alignment descriptor
 */
#define DESC_MASK		((1 << ALIGNMENT_DESC_BITS) - 1)
#define alignment_of(t)		offsetof(struct { char c; t x; }, x)
#define alignment_desc_of(t)	(((uint64_t)alignment_of(t) - 1) & DESC_MASK)
#define alignment_desc()\
(alignment_desc_of(char)	<<  0 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(short)	<<  1 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(int)		<<  2 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long)	<<  3 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long long)	<<  4 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(size_t)	<<  5 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(off_t)	<<  6 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(float)	<<  7 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(double)	<<  8 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long double)	<<  9 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(void *)	<< 10 * ALIGNMENT_DESC_BITS)

#define POOL_FEAT_ZERO		0x0000U

static const features_t features_zero =
	{POOL_FEAT_ZERO, POOL_FEAT_ZERO, POOL_FEAT_ZERO};

/*
 * compat features
 */
#define POOL_FEAT_CHECK_BAD_BLOCKS	0x0001U	/* check bad blocks in a pool */

#define POOL_FEAT_COMPAT_ALL \
	(POOL_FEAT_CHECK_BAD_BLOCKS)

#define FEAT_COMPAT(X) \
	{POOL_FEAT_##X, POOL_FEAT_ZERO, POOL_FEAT_ZERO}

/*
 * incompat features
 */
#define POOL_FEAT_SINGLEHDR	0x0001U	/* pool header only in the first part */
#define POOL_FEAT_CKSUM_2K	0x0002U	/* only first 2K of hdr checksummed */
#define POOL_FEAT_SDS		0x0004U	/* check shutdown state */

#define POOL_FEAT_INCOMPAT_ALL \
	(POOL_FEAT_SINGLEHDR | POOL_FEAT_CKSUM_2K | POOL_FEAT_SDS)

/*
 * incompat features effective values (if applicable)
 */
#ifdef SDS_ENABLED
#define POOL_E_FEAT_SDS		POOL_FEAT_SDS
#else
#define POOL_E_FEAT_SDS		0x0000U	/* empty */
#endif

#define POOL_FEAT_COMPAT_VALID \
	(POOL_FEAT_CHECK_BAD_BLOCKS)

#define POOL_FEAT_INCOMPAT_VALID \
	(POOL_FEAT_SINGLEHDR | POOL_FEAT_CKSUM_2K | POOL_E_FEAT_SDS)

#if defined(_WIN32) || NDCTL_ENABLED
#define POOL_FEAT_INCOMPAT_DEFAULT \
	(POOL_FEAT_CKSUM_2K | POOL_E_FEAT_SDS)
#else
/*
 * shutdown state support on Linux requires root access on kernel < 4.20 with
 * ndctl < 63 so it is disabled by default
 */
#define POOL_FEAT_INCOMPAT_DEFAULT \
	(POOL_FEAT_CKSUM_2K)
#endif

#if NDCTL_ENABLED
#define POOL_FEAT_COMPAT_DEFAULT \
	(POOL_FEAT_CHECK_BAD_BLOCKS)
#else
#define POOL_FEAT_COMPAT_DEFAULT \
	(POOL_FEAT_ZERO)
#endif

#define FEAT_INCOMPAT(X) \
	{POOL_FEAT_ZERO, POOL_FEAT_##X, POOL_FEAT_ZERO}

#define POOL_FEAT_VALID \
	{POOL_FEAT_COMPAT_VALID, POOL_FEAT_INCOMPAT_VALID, POOL_FEAT_ZERO}

/*
 * defines the first not checksummed field - all fields after this will be
 * ignored during checksum calculations.
 */
#define POOL_HDR_CSUM_2K_END_OFF offsetof(struct pool_hdr, unused2)
#define POOL_HDR_CSUM_4K_END_OFF offsetof(struct pool_hdr, checksum)

/*
 * pick the first not checksummed field. 2K variant is used if
 * POOL_FEAT_CKSUM_2K incompat feature is set.
 */
#define POOL_HDR_CSUM_END_OFF(hdrp) \
	((hdrp)->features.incompat & POOL_FEAT_CKSUM_2K) \
		? POOL_HDR_CSUM_2K_END_OFF : POOL_HDR_CSUM_4K_END_OFF

/* ignore shutdown state if incompat feature is disabled */
#define IGNORE_SDS(hdrp) \
	(((hdrp) != NULL) && (((hdrp)->features.incompat & POOL_FEAT_SDS) == 0))

#ifdef __cplusplus
}
#endif

#endif
