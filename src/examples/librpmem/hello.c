// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * hello.c -- hello world for librpmem
 */
#include <assert.h>
#include <errno.h>
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

#define LANG_NUM	(sizeof(hello_str) / sizeof(hello_str[0]))

#define STR_SIZE	100

struct hello_t {
	enum lang_t lang;
	char str[STR_SIZE];
};

#define POOL_SIZE	(32 * 1024 * 1024)
#define DATA_OFF	4096 /* rpmem header size */
#define NLANES		4
#define DATA_SIZE	(sizeof(struct hello_t))

static inline void
write_hello_str(struct hello_t *hello, enum lang_t lang)
{
	hello->lang = lang;
	strncpy(hello->str, hello_str[hello->lang], STR_SIZE);
}

static void
translate(struct hello_t *hello)
{
	printf("translating...\n");
	enum lang_t lang = (enum lang_t)((hello->lang + 1) % LANG_NUM);
	write_hello_str(hello, lang);
}

static int
remote_write(RPMEMpool *rpp)
{
	printf("write message to the target...\n");
	if (rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0)) {
		printf("upload failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static int
remote_read(RPMEMpool *rpp, void *buff)
{
	printf("read message from the target...\n");
	if (rpmem_read(rpp, buff, DATA_OFF, DATA_SIZE, 0)) {
		printf("download failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static RPMEMpool *
remote_open(const char *target, const char *poolset, void *pool,
		int *created)
{
	struct rpmem_pool_attr pool_attr;
	unsigned nlanes = NLANES;
	RPMEMpool *rpp;

	/* fill pool_attributes */
	memset(&pool_attr, 0, sizeof(pool_attr));
	strncpy(pool_attr.signature, POOL_SIGNATURE, RPMEM_POOL_HDR_SIG_LEN);

	/* create a remote pool */
	rpp = rpmem_create(target, poolset, pool, POOL_SIZE, &nlanes,
			&pool_attr);
	if (rpp) {
		memset(pool, 0, POOL_SIZE);
		*created = 1;
		return rpp;
	}

	if (errno != EEXIST) {
		fprintf(stderr, "rpmem_create: %s\n", rpmem_errormsg());
		return NULL;
	}

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

	*created = 0;
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
parse_args(int argc, char *argv[], const char **target, const char **poolset)
{
	if (argc < 3) {
		fprintf(stderr, "usage:\t%s <target> <poolset>\n"
				"\n"
				"e.g.:\t%s localhost pool.set\n",
				argv[0], argv[0]);

		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
}

static void *
alloc_memory()
{
	long pagesize = sysconf(_SC_PAGESIZE);
	if (pagesize < 0) {
		perror("sysconf");
		return NULL;
	}

	/* allocate a page size aligned local memory pool */
	void *mem;
	int ret = posix_memalign(&mem, pagesize, POOL_SIZE);
	if (ret) {
		fprintf(stderr, "posix_memalign: %s\n", strerror(ret));
		return NULL;
	}

	return mem;
}

int
main(int argc, char *argv[])
{
	const char *target, *poolset;
	parse_args(argc, argv, &target, &poolset);

	void *pool = alloc_memory();
	if (!pool)
		exit(1);

	struct hello_t *hello = (struct hello_t *)(pool + DATA_OFF);
	int created;
	int ret;

	RPMEMpool *rpp = remote_open(target, poolset, pool, &created);
	if (!rpp) {
		ret = 1;
		goto exit_free;
	}

	if (created) {
		write_hello_str(hello, en);
	} else {
		ret = remote_read(rpp, hello);
		if (ret)
			goto exit_close;
		printf("\n%s\n\n", hello->str);
		translate(hello);
	}

	ret = remote_write(rpp);
	if (ret)
		goto exit_close;
	printf("rerun application to read the translation.\n");

exit_close:
	/* close the remote pool */
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

exit_free:
	free(pool);
	return ret;
}
