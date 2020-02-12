// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * sys/mman.h -- memory-mapped files for Windows
 */

#ifndef SYS_MMAN_H
#define SYS_MMAN_H 1

#ifdef __cplusplus
extern "C" {
#endif

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

void *mmap(void *addr, size_t len, int prot, int flags,
	int fd, os_off_t offset);
int munmap(void *addr, size_t len);
int msync(void *addr, size_t len, int flags);

int mprotect(void *addr, size_t len, int prot);

#ifdef __cplusplus
}
#endif

#endif /* SYS_MMAN_H */
