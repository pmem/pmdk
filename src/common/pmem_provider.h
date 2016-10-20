/*
 * Copyright 2016, Intel Corporation
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
 * pmem_provider.h -- definitions for persistent memory provider interface
 */

#ifndef PMEM_PROVIDER_H
#define PMEM_PROVIDER_H 1

#include "util.h"
#include "file.h"

enum pmem_provider_type {
	PMEM_PROVIDER_UNKNOWN,
	PMEM_PROVIDER_REGULAR_FILE,
	PMEM_PROVIDER_DEVICE_DAX,

	MAX_PMEM_PROVIDER_TYPE
};

struct pmem_provider {
	char *path;
	int fd;
	util_stat_t st;
	int exists;

	enum pmem_provider_type type;
	const struct pmem_provider_ops *pops;
};

struct pmem_provider_ops {
	int (*type_match)(struct pmem_provider *p);
	int (*open)(struct pmem_provider *p, int flags, mode_t mode, int tmp);
	void (*close)(struct pmem_provider *p);
	void (*unlink)(struct pmem_provider *p);
	int (*lock)(struct pmem_provider *p);
	void *(*map)(struct pmem_provider *p, size_t alignment);
	ssize_t (*get_size)(struct pmem_provider *p);
	int (*allocate_space)(struct pmem_provider *p, size_t size, int sparse);
	int (*always_pmem)(void);
};

int pmem_provider_init(struct pmem_provider *p, const char *path);
void pmem_provider_fini(struct pmem_provider *p);

void pmem_provider_type_register(enum pmem_provider_type type,
	struct pmem_provider_ops *ops);

#ifndef _MSC_VER

#define PMEM_PROVIDER_TYPE(n, ops)\
__attribute__((constructor)) static void _pmem_provider_##n(void)\
{ pmem_provider_type_register(n, ops); }

#else

#define PMEM_PROVIDER_TYPE(n, ops)\
static void _pmem_provider_##n(void)\
{ pmem_provider_type_register(n, ops); }\
MSVC_CONSTR(_pmem_provider_##n)

#endif

#endif /* PMEM_PROVIDER_H */
