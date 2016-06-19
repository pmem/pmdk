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
 * manpage.c -- example for librpmem manpage
 */
#include <stdio.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIZE	(32 * 1024 * 1024)
#define NLANES		4
unsigned char pool[POOL_SIZE];

int
main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <target> <pool set>\n", argv[0]);
		return 1;
	}

	int ret;
	unsigned nlanes = NLANES;
	const char *target = argv[1];
	const char *pool_set_name = argv[2];

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	RPMEMpool *rpp = rpmem_create(target, pool_set_name,
		pool, POOL_SIZE, &nlanes, &pool_attr);

	if (!rpp) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return 1;
	}

	memset(pool, 0, POOL_SIZE);

	ret = rpmem_persist(rpp, 0, POOL_SIZE, 0);
	if (ret) {
		fprintf(stderr, "rpmem_persist: %s\n", rpmem_errormsg());
		return 1;
	}

	ret = rpmem_close(rpp);
	if (ret) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		return 1;
	}

	return 0;
}
