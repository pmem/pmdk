/*
 * Copyright 2019, Intel Corporation
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
 * pmem2_utils.h -- libpmem2 utilities functions
 */

#ifndef PMEM2_UTILS_H
#define PMEM2_UTILS_H 1

#include <errno.h>

#include "os.h"

#define PMEM2_E_ERRNO (-errno)

void *pmem2_malloc(size_t size, int *err);

#ifdef _WIN32
int pmem2_lasterror_to_err();
#endif

enum pmem2_file_type {
	PMEM2_FTYPE_REG = 1,
	PMEM2_FTYPE_DEVDAX = 2,
	PMEM2_FTYPE_DIR = 3,
};

int pmem2_get_type_from_stat(const os_stat_t *st, enum pmem2_file_type *type);
int pmem2_device_dax_size_from_stat(const os_stat_t *st, size_t *size);
int pmem2_device_dax_alignment_from_stat(const os_stat_t *st,
		size_t *alignment);

#endif /* PMEM2_UTILS_H */
