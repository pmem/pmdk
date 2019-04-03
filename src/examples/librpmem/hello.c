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
 * hello.c -- hello world for librpmem
 */
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <librpmem.h>

#define POOL_SIGNATURE	"HELLO"

enum lang_t {en, es};

static const char *hello_str[] = {
	[en] = "Hello world!",
	[es] = "¡Hola Mundo!"
};

#define LANG_NUM	(sizeof(hello_str) / sizeof(char *))

#define STR_SIZE	100

struct hello_t {
	enum lang_t lang;
	char str[STR_SIZE];
};

#define POOL_SIZE	(32 * 1024 * 1024)
#define DATA_OFF	4096 /* rpmem header size */
#define NLANES		4
#define DATA_SIZE	(sizeof(struct hello_t))

static void
translate(struct hello_t *hello, int start)
{
	printf("translating...\n");
	hello->lang = (enum lang_t)((start + 1) % LANG_NUM);
	strncpy(hello->str, hello_str[hello->lang], STR_SIZE);
}

static int
upload(RPMEMpool *rpp)
{
	printf("uploading translation to the target...\n");
	if (rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0)) {
		printf("upload failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static int
download(RPMEMpool *rpp, void *buff)
{
	printf("download translation from the target...\n");
	if (rpmem_read(rpp, buff, DATA_OFF, DATA_SIZE, 0)) {
		printf("download failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

/*
 * remote_create_or_open -- create or open the remote replica
 */
static RPMEMpool *
remote_create_or_open(const char *target, const char *poolset, void *pool,
		int *created)
{
	struct rpmem_pool_attr pool_attr;
	unsigned nlanes = NLANES;
	*created = 1;
	RPMEMpool *rpp;

	/* fill pool_attributes */
	memset(&pool_attr, 0, sizeof(pool_attr));
	strncpy(pool_attr.signature, POOL_SIGNATURE, RPMEM_POOL_HDR_SIG_LEN);

	/* create a remote pool */
	rpp = rpmem_create(target, poolset, pool, POOL_SIZE, &nlanes,
			&pool_attr);
	if (rpp) {
		memset(pool, 0, POOL_SIZE);
		return rpp;
	}

	if (errno != EEXIST) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return NULL;
	}

	/* the remote pool exists */
	*created = 0;

	/* open a remote pool */
	rpp = rpmem_open(target, poolset, pool, POOL_SIZE, &nlanes,
			&pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_open: %s\n", rpmem_errormsg());
		return NULL;
	}

	/* verify signature */
	if (strcmp(pool_attr.signature, POOL_SIGNATURE) != 0) {
		fprintf(stderr, "invalid signature\n");
		goto err;
	}

	return rpp;

err:
	/* close the remote pool */
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

	return NULL;
}

static void
parse_args(int argc, char *argv[], const char **target, const char **poolset,
		long *seed)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <target> <poolset> [<seed>]\n",
				argv[0]);
		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
	if (argc > 3)
		*seed = strtol(argv[3], NULL, 10);
}

static void *
alloc_memory()
{
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0) {
		perror("sysconf");
		exit(1);
	}

	/* allocate a page size aligned local memory pool */
	void *mem;
	int ret = posix_memalign(&mem, pagesize, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memaling: %s\n", strerror(ret));
		exit(1);
	}

	assert(mem != NULL);

	return mem;
}

int
main(int argc, char *argv[])
{
	const char *target, *poolset;
	long seed = time(0);
	parse_args(argc, argv, &target, &poolset, &seed);

	void *pool = alloc_memory();
	struct hello_t *hello = (struct hello_t *)(pool + DATA_OFF);
	int created, lang;
	int ret;

	RPMEMpool *rpp = remote_create_or_open(target, poolset, pool, &created);
	if (!rpp) {
		ret = 1;
		goto err;
	}

	if (created) {
		/* pick initial language randomly */
		srand(seed);
		lang = rand();
	} else {
		ret = download(rpp, hello);
		if (ret)
			goto err_close;
		printf("\n%s\n\n", hello->str);
		lang = hello->lang;
	}

	translate(hello, lang);
	ret = upload(rpp);
	if (ret)
		goto err_close;
	printf("rerun application to read the translation.\n");

err_close:
	/* close the remote pool */
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

err:
	free(pool);
	return ret;
}
