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
 * pool_hdr.h -- internal definitions for pool header module
 */

#ifndef PMDK_POOL_HDR_H
#define PMDK_POOL_HDR_H 1

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include "uuid.h"

#ifdef __cplusplus
extern "C" {
#endif

/* possible values of the machine class field in the above struct */
#define PMDK_MACHINE_CLASS_64 2 /* 64 bit pointers, 64 bit size_t */

/* possible values of the machine field in the above struct */
#define PMDK_MACHINE_X86_64 62
#define PMDK_MACHINE_AARCH64 183

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
	unsigned char unused[1920];	/* must be zero */
	/* not checksumed */
	unsigned char unused2[1976];	/* must be zero */
	uint64_t padding[8];		/* !shutdown status */
	uint64_t checksum;		/* checksum of above fields */
};

#define POOL_HDR_SIZE	(sizeof(struct pool_hdr))

#define POOL_DESC_SIZE 4096

void util_convert2le_hdr(struct pool_hdr *hdrp);
void util_convert2h_hdr_nocheck(struct pool_hdr *hdrp);

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

#ifdef __cplusplus
}
#endif

#endif
