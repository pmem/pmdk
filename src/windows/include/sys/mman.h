/*
 * Copyright 2015-2016, Intel Corporation
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
 * sys/mman.h -- memory-mapped files for Windows
 */

/*
 * XXX - see mmap_windows.c for details
 */

#ifndef SYS_MMAN_H
#define SYS_MMAN_H 1

#define PROT_NONE	0x0
#define PROT_READ	0x1
#define PROT_WRITE	0x2
#define PROT_EXEC	0x4

#define MAP_SHARED	0x1
#define MAP_PRIVATE	0x2

#define MAP_FIXED	0x10
#define MAP_ANONYMOUS	0x20
#define MAP_ANON	MAP_ANONYMOUS

#define MAP_NORESERVE	0x04000

#define MS_ASYNC	1
#define MS_SYNC		4
#define MS_INVALIDATE	2

#define MAP_FAILED ((void *)(-1))


void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t len);
int msync(void *addr, size_t len, int flags);

int mprotect(void *addr, size_t len, int prot);

#endif /* SYS_MMAN_H */
