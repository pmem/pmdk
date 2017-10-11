/*
 * Copyright 2014-2017, Intel Corporation
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

#ifndef NVML_POOL_HDR_H
#define NVML_POOL_HDR_H 1

#include <stdint.h>
#include "uuid.h"

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
 * The alignment of each type is computer as an offset of field
 * of specific type in the following structure:
 * struct {
 *	char byte;
 *	type field;
 * };
 *
 * The value is decremented by 1 and masked by 4 bits.
 * Multiple alignment are stored on consecutive 4 bits of each
 * type in order specified above.
 */
struct arch_flags {
	uint64_t alignment_desc;	/* alignment descriptor */
	uint8_t ei_class;		/* ELF format file class */
	uint8_t ei_data;		/* ELF format data encoding */
	uint8_t reserved[4];
	uint16_t e_machine;		/* required architecture */
};

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
	unsigned char unused[3944];	/* must be zero */
	uint64_t checksum;		/* checksum of above fields */
};

#define POOL_HDR_SIZE	(sizeof(struct pool_hdr))

#define POOL_DESC_SIZE 4096

void util_convert2le_hdr(struct pool_hdr *hdrp);
void util_convert2h_hdr_nocheck(struct pool_hdr *hdrp);
int util_convert_hdr(struct pool_hdr *hdrp);
int util_convert_hdr_remote(struct pool_hdr *hdrp);
int util_get_arch_flags(struct arch_flags *arch_flags);
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

#endif
