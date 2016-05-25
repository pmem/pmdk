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
 * basic.c -- basic example for the librpmem
 */
#include <stdio.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIZE	(32 * 1024 * 1024)
#define NLANES		4
#define SET_POOLSET_UUID 1
#define SET_UUID 2
#define SET_NEXT 3
#define SET_PREV 4

unsigned char pool[POOL_SIZE];

/*
 * default_attr -- fill pool attributes to default values
 */
static void
default_attr(struct rpmem_pool_attr *attr)
{
	memset(attr, 0, sizeof(*attr));
	attr->major = 1;
	strncpy(attr->signature, "EXAMPLE", RPMEM_POOL_HDR_SIG_LEN);
	memset(attr->poolset_uuid, SET_POOLSET_UUID, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->uuid, SET_UUID, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->next_uuid, SET_NEXT, RPMEM_POOL_HDR_UUID_LEN);
	memset(attr->prev_uuid, SET_PREV, RPMEM_POOL_HDR_UUID_LEN);
}

int
main(int argc, char *argv[])
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s [create|remove|open]"
			" <target> <pool_set> [options]\n", argv[0]);
		return 1;
	}
	char *op = argv[1];
	char *target = argv[2];
	char *pool_set = argv[3];
	unsigned nlanes = NLANES;

	if (strcmp(op, "create") == 0) {
		struct rpmem_pool_attr pool_attr;
		default_attr(&pool_attr);

		RPMEMpool *rpp = rpmem_create(target, pool_set,
			pool, POOL_SIZE, &nlanes, &pool_attr);

		if (!rpp) {
			fprintf(stderr, "rpmem_create: %s\n",
					rpmem_errormsg());
			return 1;
		}

		int ret = rpmem_close(rpp);
		if (ret)
			fprintf(stderr, "rpmem_close: %s\n",
					rpmem_errormsg());

		return ret;
	} else if (strcmp(op, "open") == 0) {
		struct rpmem_pool_attr def_attr;
		default_attr(&def_attr);

		struct rpmem_pool_attr pool_attr;
		RPMEMpool *rpp = rpmem_open(target, pool_set,
			pool, POOL_SIZE, &nlanes, &pool_attr);
		if (!rpp) {
			fprintf(stderr, "rpmem_open: %s\n",
					rpmem_errormsg());
			return 1;
		}

		if (memcmp(&def_attr, &pool_attr, sizeof(def_attr))) {
			fprintf(stderr, "remote pool not consistent\n");
		}

		int ret = rpmem_close(rpp);
		if (ret)
			fprintf(stderr, "rpmem_close: %s\n",
					rpmem_errormsg());

		return ret;
	} else if (strcmp(op, "remove") == 0) {
		int ret = rpmem_remove(target, pool_set);

		if (ret)
			fprintf(stderr, "rpmem_remove: %s\n",
					rpmem_errormsg());

		return ret;
	} else {
		fprintf(stderr, "unsupported operation -- '%s'\n", op);
		return 1;
	}
}
