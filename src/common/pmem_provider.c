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
 * pmem_provider.c -- persistent memory provider interface
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/file.h>

#include "pmem_provider.h"
#include "mmap.h"
#include "out.h"

static struct pmem_provider_ops *
pmem_provider_operations[MAX_PMEM_PROVIDER_TYPE] = {NULL};

/*
 * pmem_provider_type_register -- adds a new type to the pmem providers
 */
void
pmem_provider_type_register(enum pmem_provider_type type,
	struct pmem_provider_ops *ops)
{
	pmem_provider_operations[type] = ops;
}

/*
 * pmem_provider_query_type -- (internal) checks the type of a pmem provider
 */
static enum pmem_provider_type
pmem_provider_query_type(struct pmem_provider *p)
{
	for (enum pmem_provider_type type = PMEM_PROVIDER_UNKNOWN;
		type < MAX_PMEM_PROVIDER_TYPE; ++type) {
		if (pmem_provider_operations[type] &&
			pmem_provider_operations[type]->type_match(p))
			return type;
	}

	return PMEM_PROVIDER_UNKNOWN;
}

/*
 * pmem_file_init -- initializes an instance of peristent memory provider
 */
int
pmem_provider_init(struct pmem_provider *p, const char *path)
{
	p->path = Strdup(path);
	if (p->path == NULL)
		goto error_path_strdup;

	p->exists = 1;
	int olderrno = errno;
	if (util_stat(path, &p->st) < 0) {
		if (errno == ENOENT)
			p->exists = 0;
		else
			goto error_init;
	}
	errno = olderrno; /* file not existing is not an error */

	p->type = pmem_provider_query_type(p);
	if (p->type == PMEM_PROVIDER_UNKNOWN)
		goto error_init;

	ASSERTne(p->type, MAX_PMEM_PROVIDER_TYPE);

	p->pops = pmem_provider_operations[p->type];

	return 0;

error_init:
	Free(p->path);
error_path_strdup:
	return -1;
}

/*
 * pmem_provider_fini -- cleanups an instance of persistent memory provider
 */
void
pmem_provider_fini(struct pmem_provider *p)
{
	Free(p->path);
	p->path = NULL;
}
