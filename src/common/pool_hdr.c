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
