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

#include <errno.h>

#include "pmem2_utils_posix.h"
#include "libpmem2.h"

int
pmem2_errno_to_err(void)
{
	/* XXX convert it to an array */
	if (errno == EIO)
		return PMEM2_E_IO;
	if (errno == ELOOP)
		return PMEM2_E_LOOP;
	if (errno == ENAMETOOLONG)
		return PMEM2_E_NAME_TOO_LONG;
	if (errno == ENOTDIR)
		return PMEM2_E_NOT_DIR;
	if (errno == ENOMEM)
		return PMEM2_E_OUT_OF_MEMORY;
	if (errno == ENOENT)
		return PMEM2_E_NO_ENTRY;
	if (errno == EACCES)
		return PMEM2_E_ACCESS;
	if (errno == ENFILE)
		return PMEM2_E_NFILE;
	if (errno == EMFILE)
		return PMEM2_E_MFILE;
	if (errno == EPERM)
		return PMEM2_E_PERM;
	if (errno == ERANGE)
		return PMEM2_E_RANGE;

	return PMEM2_E_UNKNOWN;
}
