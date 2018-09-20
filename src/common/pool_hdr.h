/*
 * Copyright 2014-2018, Intel Corporation
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
 * pool_hdr.h -- internal definitions for pool header module
 */

#ifndef PMDK_POOL_HDR_H
#define PMDK_POOL_HDR_H 1

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include "uuid.h"
#include "shutdown_state.h"
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

/* possible values of the data field in the above struct */
#define PMDK_DATA_LE 1 /* 2's complement, little endian */
#define PMDK_DATA_BE 2 /* 2's complement, big endian */

/*
 * header used at the beginning of all types of memory pools
 *
 * for pools build on persistent memory, the integer types
 * below are stored in little-endian byte order.
 */
#define POOL_HDR_SIG_LEN 8

struct pool_hdr {
	char signature[POOL_HDR_SIG_LEN];
	uint32_t major;			/* format major version number */
	uint32_t compat_features;	/* mask: compatible "may" features */
	uint32_t incompat_features;	/* mask: "must support" features */
	uint32_t ro_compat_features;	/* mask: force RO if unsupported */
	uuid_t poolset_uuid; /* pool set UUID */
	uuid_t uuid; /* UUID of this file */
	uuid_t prev_part_uuid; /* prev part */
	uuid_t next_part_uuid; /* next part */
	uuid_t prev_repl_uuid; /* prev replica */
	uuid_t next_repl_uuid; /* next replica */
	uint64_t crtime;		/* when created (seconds since epoch) */
	struct arch_flags arch_flags;	/* architecture identification flags */
	unsigned char unused[1888];	/* must be zero */
	/* not checksumed */
	unsigned char unused2[1992];	/* must be zero */
	struct shutdown_state sds;	/* shutdown status */
	uint64_t checksum;		/* checksum of above fields */
};

#define POOL_HDR_SIZE	(sizeof(struct pool_hdr))

#define POOL_DESC_SIZE 4096

void util_convert2le_hdr(struct pool_hdr *hdrp);
void util_convert2h_hdr_nocheck(struct pool_hdr *hdrp);

void util_get_arch_flags(struct arch_flags *arch_flags);
int util_check_arch_flags(const struct arch_flags *arch_flags);

int util_feature_check(struct pool_hdr *hdrp, uint32_t incompat,
				uint32_t ro_compat, uint32_t compat);

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

/*
 * incompat features
 */
#define POOL_FEAT_SINGLEHDR	0x0001	/* pool header only in the first part */
#define POOL_FEAT_CKSUM_2K	0x0002	/* only first 2K of hdr checksummed */
#define POOL_FEAT_SDS		0x0004	/* check shutdown state */

#define POOL_FEAT_ALL \
	(POOL_FEAT_SINGLEHDR | POOL_FEAT_CKSUM_2K | POOL_FEAT_SDS)

/*
 * incompat features effective values (if applicable)
 */
#ifdef SDS_ENABLED
#define POOL_E_FEAT_SDS		POOL_FEAT_SDS
#else
#define POOL_E_FEAT_SDS		0x0000	/* empty */
#endif

#define POOL_FEAT_VALID \
	(POOL_FEAT_SINGLEHDR | POOL_FEAT_CKSUM_2K | POOL_E_FEAT_SDS)

#define POOL_FEAT_DEFAULT \
	(POOL_FEAT_CKSUM_2K | POOL_E_FEAT_SDS)

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
	((hdrp)->incompat_features & POOL_FEAT_CKSUM_2K) \
		? POOL_HDR_CSUM_2K_END_OFF : POOL_HDR_CSUM_4K_END_OFF

/* ignore shutdown state if incompat feature is disabled */
#define IGNORE_SDS(hdrp) \
	(((hdrp) != NULL) && (((hdrp)->incompat_features & POOL_FEAT_SDS) == 0))

#endif
