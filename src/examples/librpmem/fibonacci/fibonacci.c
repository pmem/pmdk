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
 * fibonacci.c -- fibonacci sequence generator for librpmem
 */
#include <assert.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libpmem.h>
#include <librpmem.h>

#define POOL_SIGNATURE	"FIBO"
#define FIBO_SIG_LEN RPMEM_POOL_HDR_SIG_LEN
#define SIZE_1MB	(1 * 1024 * 1024)

struct fibo_t {
	char signature[FIBO_SIG_LEN];
	unsigned n;	/* index */
	uint64_t fn;	/* F_{n} */
	uint64_t fn1;	/* F_{n + 1} */
	int checksum;
};

#define POOL_SIZE	(size_t)(32 * 1024 * 1024)
#define DATA_OFF	4096 /* rpmem header size */
#define NLANES		4
#define DATA_SIZE	(sizeof(struct fibo_t))

#define GET_FIBO(pool)	((struct fibo_t *)(pool + DATA_OFF))

#define BROKEN_LOCAL	(1 << 0)
#define BROKEN_REMOTE	(1 << 0)

static void parse_args(int argc, char *argv[], const char **target,
		const char **poolset, const char **path);
static void *map_pmem(const char *path, size_t *mapped_len);

static RPMEMpool *remote_create_or_open(const char *target, const char *poolset,
		void *pool, int *created);
static int remote_write(RPMEMpool *rpp);
static int remote_read(RPMEMpool *rpp, void *buff);

static void fibo_cleanup(RPMEMpool *rpp, void *pool, struct fibo_t *fibo_r,
		int *initialized);
static void fibo_generate(void *pool);
static void fibo_print(void *pool);

int
main(int argc, char *argv[])
{
	const char *target, *poolset, *path;
	parse_args(argc, argv, &target, &poolset, &path);

	struct fibo_t fibo_r; /* copy from remote */
	int created; /* remote: 1 == created, 0 == opened */
	int initialized; /* sequence: 1 == initialized, 0 = con be continued */
	int ret;

	/* map local pool */
	size_t mapped_len;
	void *pool = map_pmem(path, &mapped_len);

	/* open remote pool */
	RPMEMpool *rpp = remote_create_or_open(target, poolset, pool, &created);
	if (!rpp) {
		ret = 1;
		goto unmap;
	}

	if (!created) {
		/* restore from remote */
		ret = remote_read(rpp, &fibo_r);
		if (ret)
			/* invalidate remote copy */
			memset(&fibo_r, 1, DATA_SIZE);
	} else
		/* zero remote copy */
		memset(&fibo_r, 0, DATA_SIZE);

	/* validate copies from local and remote */
	fibo_cleanup(rpp, pool, &fibo_r, &initialized);
	/* generate new number */
	if (!initialized)
		fibo_generate(pool);
	fibo_print(pool);

	/* store to remote */
	ret = remote_write(rpp);
	if (ret)
		goto err;

	printf("rerun application to generate next Fibonacci number.\n");

err:
	/* close the remote pool */
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
	}

unmap:
	pmem_unmap(pool, mapped_len);

	return ret;
}

static void
parse_args(int argc, char *argv[], const char **target, const char **poolset,
		const char **path)
{
	if (argc < 4) {
		fprintf(stderr, "usage:\t%s <target> <poolset> <path>\n",
				argv[0]);
		exit(1);
	}

	*target = argv[1];
	*poolset = argv[2];
	*path = argv[3];
}

static void *
map_pmem(const char *path, size_t *mapped_len)
{
	int is_pmem;
	void *pmem = pmem_map_file(path, 0, 0, 0, mapped_len, &is_pmem);
	if (!pmem)
		return NULL;

	if (!is_pmem) {
		fprintf(stderr, "is not persistent memory: %s\n", path);
		goto err;
	}

	if (*mapped_len < POOL_SIZE) {
		fprintf(stderr, "too small: %ld < %ld\n", *mapped_len,
				POOL_SIZE);
		goto err;
	}

	return pmem;
err:
	pmem_unmap(pmem, *mapped_len);
	exit(1);
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
	if (rpp)
		goto verify;
	else {
		if (errno != EEXIST) {
			fprintf(stderr, "rpmem_create: %s\n",
					rpmem_errormsg());
			return NULL;
		}
		/* the remote pool exists */
		*created = 0;
	}

	/* open a remote pool */
	rpp = rpmem_open(target, poolset, pool, POOL_SIZE, &nlanes,
			&pool_attr);
	if (!rpp) {
		fprintf(stderr, "rpmem_open: %s\n", rpmem_errormsg());
		return NULL;
	}

verify:
	/* verify signature */
	if (strcmp(pool_attr.signature, POOL_SIGNATURE) != 0) {
		pool_attr.signature[RPMEM_POOL_HDR_SIG_LEN - 1] = '\0';
		fprintf(stderr, "invalid signature: %s != %s\n",
				pool_attr.signature, POOL_SIGNATURE);
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

static int
remote_write(RPMEMpool *rpp)
{
	printf("storing Fibonacci numbers on the target...\n");
	if (rpmem_persist(rpp, DATA_OFF, DATA_SIZE, 0, 0)) {
		printf("store failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static int
remote_read(RPMEMpool *rpp, void *buff)
{
	printf("restore Fibonacci numbers from the target...\n");
	if (rpmem_read(rpp, buff, DATA_OFF, DATA_SIZE, 0)) {
		printf("restore failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static int
fibo_checksum(struct fibo_t *fibo)
{
	return fibo->signature[0] + fibo->fn + fibo->fn1;
}

static void
fibo_init(struct fibo_t *fibo)
{
	printf("initializing...\n");
	memset(fibo, 0, DATA_SIZE);
	strncpy(fibo->signature, POOL_SIGNATURE, FIBO_SIG_LEN);
	fibo->n = 0;
	fibo->fn = 0;
	fibo->fn1 = 1;
	fibo->checksum = fibo_checksum(fibo);
}

static int
fibo_is_valid(struct fibo_t *fibo)
{
	if (strncmp(fibo->signature, POOL_SIGNATURE, FIBO_SIG_LEN) != 0)
		return 0;

	return fibo->checksum == fibo_checksum(fibo);
}

static int
fibo_is_zeroed(struct fibo_t *fibo)
{
	char *data = (char *)fibo;
	for (size_t i = 0; i < DATA_SIZE; ++i)
		if (data[i] != 0)
			return 0;

	return 1;
}

/*
 * fibo_validate -- validate local and remote copies of the F sequence
 */
static struct fibo_t *
fibo_validate(struct fibo_t *fibo, struct fibo_t *fibo_r, unsigned *state)
{
	struct fibo_t *valid = NULL;

	if (fibo_is_valid(fibo))
		valid = fibo;
	else if (!fibo_is_zeroed(fibo)) {
		fprintf(stderr, "broken local memory pool!\n");
		*state |= BROKEN_LOCAL;
	}

	if (fibo_is_valid(fibo_r)) {
		if (!valid)
			valid = fibo_r;
		else if (fibo_r->n > valid->n)
			valid = fibo_r;
	} else if (!fibo_is_zeroed(fibo_r)) {
		fprintf(stderr, "broken remote memory pool!\n");
		*state |= BROKEN_REMOTE;
	}

	if (!valid)
		printf("no valid Fibonacci numbers found.\n");

	return valid;
}

/*
 * fibo_cleanup -- based on validation outcome cleanup copies and initialize
 * if required
 */
static void
fibo_cleanup(RPMEMpool *rpp, void *pool, struct fibo_t *fibo_r,
		int *initialized)
{
	struct fibo_t *fibo = GET_FIBO(pool);
	struct fibo_t temp;
	unsigned state = 0;
	int ret;

	struct fibo_t *valid = fibo_validate(fibo, fibo_r, &state);

	/* store valid fibonacci data in temp */
	if (valid)
		memcpy(&temp, valid, sizeof(temp));

	/* local cleanup */
	if (state & (BROKEN_LOCAL | BROKEN_REMOTE))
		memset(pool, 0, POOL_SIZE);

	/* remote cleanup */
	if (state & BROKEN_REMOTE) {
		ret = rpmem_persist(rpp, DATA_OFF, POOL_SIZE - DATA_OFF, 0, 0);
		if (ret) {
			printf("remote cleanup failed: %s\n", rpmem_errormsg());
		}
	}

	if (valid) {
		/* restore from temp to local */
		memcpy(fibo, &temp, sizeof(temp));
		*initialized = 0;
	} else {
		/* init local */
		fibo_init(fibo);
		*initialized = 1;
	}

	/* no matter what local is valid now */
}

/*
 * fibo_generate -- generate next Fibonacci number
 */
static void
fibo_generate(void *pool)
{
	struct fibo_t *fibo = GET_FIBO(pool);

	uint64_t fn2 = fibo->fn1 + fibo->fn;
	if (fn2 < fibo->fn1) {
		printf("overflow detected!\n");
		fibo_init(fibo);
		return;
	}
	fibo->fn = fibo->fn1;
	fibo->fn1 = fn2;
	++fibo->n;
	fibo->checksum = fibo_checksum(fibo);
}

static void
fibo_print(void *pool)
{
	struct fibo_t *fibo = GET_FIBO(pool);
	if (fibo->n == 0)
		printf("F[0] = %lu\n", fibo->fn);
	printf("F[%u] = %lu\n", fibo->n + 1, fibo->fn1);
}
