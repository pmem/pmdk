// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * win_mmap.h -- (internal) tracks the regions mapped by mmap
 */

#ifndef WIN_MMAP_H
#define WIN_MMAP_H 1

#include "queue.h"

#define roundup(x, y)	((((x) + ((y) - 1)) / (y)) * (y))
#define rounddown(x, y)	(((x) / (y)) * (y))

void win_mmap_init(void);
void win_mmap_fini(void);

/* allocation/mmap granularity */
extern unsigned long long Mmap_align;

typedef enum FILE_MAPPING_TRACKER_FLAGS {
	FILE_MAPPING_TRACKER_FLAG_DIRECT_MAPPED = 0x0001,

	/*
	 * This should hold the value of all flags ORed for debug purpose.
	 */
	FILE_MAPPING_TRACKER_FLAGS_MASK =
		FILE_MAPPING_TRACKER_FLAG_DIRECT_MAPPED
} FILE_MAPPING_TRACKER_FLAGS;

/*
 * this structure tracks the file mappings outstanding per file handle
 */
typedef struct FILE_MAPPING_TRACKER {
	PMDK_SORTEDQ_ENTRY(FILE_MAPPING_TRACKER) ListEntry;
	HANDLE FileHandle;
	HANDLE FileMappingHandle;
	void *BaseAddress;
	void *EndAddress;
	DWORD Access;
	os_off_t Offset;
	size_t FileLen;
	FILE_MAPPING_TRACKER_FLAGS Flags;
} FILE_MAPPING_TRACKER, *PFILE_MAPPING_TRACKER;

extern SRWLOCK FileMappingQLock;
extern PMDK_SORTEDQ_HEAD(FMLHead, FILE_MAPPING_TRACKER) FileMappingQHead;

#endif /* WIN_MMAP_H */
