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
	hdrp->checksum = le64toh(hdrp->checksum);
}
