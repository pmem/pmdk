// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

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

struct fibo_t {
	char signature[FIBO_SIG_LEN];
	unsigned n;	/* index */
	uint64_t fn;	/* F_{n} */
	uint64_t fn1;	/* F_{n + 1} */
	int checksum;
};

#define POOL_SIZE	(size_t)(32 * 1024 * 1024)
#define RPMEM_HDR_SIZE	4096
#define FIBO_OFF	RPMEM_HDR_SIZE
#define FIBO_SIZE	(sizeof(struct fibo_t))
#define RESERVED_SIZE	(POOL_SIZE - RPMEM_HDR_SIZE - FIBO_SIZE)

struct pool_t {
	unsigned char pool_hdr[RPMEM_HDR_SIZE];
	struct fibo_t fibo;
	unsigned char reserved[RESERVED_SIZE];
};

#define NLANES		4

#define BROKEN_LOCAL	(1 << 0)
#define BROKEN_REMOTE	(1 << 1)

static int
fibo_checksum(struct fibo_t *fibo)
{
	return fibo->signature[0] + fibo->fn + fibo->fn1;
}

static void
fibo_init(struct fibo_t *fibo)
{
	printf("initializing...\n");
	memset(fibo, 0, FIBO_SIZE);
	strncpy(fibo->signature, POOL_SIGNATURE, FIBO_SIG_LEN);
	fibo->n = 0;
	fibo->fn = 0;
	fibo->fn1 = 1;
	fibo->checksum = fibo_checksum(fibo);
	pmem_persist(fibo, FIBO_SIZE);
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
	for (size_t i = 0; i < FIBO_SIZE; ++i)
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
		fprintf(stderr, "no valid Fibonacci numbers found.\n");

	return valid;
}

/*
 * fibo_recovery -- based on validation outcome cleanup copies and initialize
 * if required
 */
static int
fibo_recovery(RPMEMpool *rpp, struct pool_t *pool, struct fibo_t *fibo_r,
		int *initialized)
{
	struct fibo_t *fibo = &pool->fibo;
	unsigned state = 0;
	int ret;

	struct fibo_t *valid = fibo_validate(fibo, fibo_r, &state);

	/* store valid fibonacci data in local */
	if (valid) {
		if (valid != fibo)
			pmem_memcpy_persist(fibo, valid, FIBO_SIZE);
		*initialized = 0;
	} else {
		/* init local */
		fibo_init(fibo);
		*initialized = 1;
	}

	/* local cleanup */
	if (state & BROKEN_LOCAL) {
		/* zero reserved parts */
		memset(pool->pool_hdr, 0, RPMEM_HDR_SIZE);
		memset(pool->reserved, 0, RESERVED_SIZE);
		pmem_persist(pool, POOL_SIZE);
	}

	/* remote cleanup */
	if (state & BROKEN_REMOTE) {
		/* replicate data + reserved */
		ret = rpmem_persist(rpp, FIBO_OFF, POOL_SIZE - FIBO_OFF, 0, 0);
		if (ret) {
			fprintf(stderr, "remote recovery failed: %s\n",
					rpmem_errormsg());
			return ret;
		}
	}

	return 0;
}

/*
 * fibo_generate -- generate next Fibonacci number
 */
static void
fibo_generate(struct fibo_t *fibo)
{
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
	pmem_persist(fibo, FIBO_SIZE);
}

static void
fibo_print(struct fibo_t *fibo)
{
	if (fibo->n == 0)
		printf("F[0] = %lu\n", fibo->fn);
	printf("F[%u] = %lu\n", fibo->n + 1, fibo->fn1);
}

/*
 * remote_create_or_open -- create or open the remote replica
 */
static RPMEMpool *
remote_create_or_open(const char *target, const char *poolset,
		struct pool_t *pool, int *created)
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

	if (errno != EEXIST) {
		fprintf(stderr, "rpmem_create: %s\n",
				rpmem_errormsg());
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

verify:
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

static int
remote_write(RPMEMpool *rpp)
{
	printf("storing Fibonacci numbers on the target...\n");
	if (rpmem_persist(rpp, FIBO_OFF, FIBO_SIZE, 0, 0)) {
		printf("store failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
}

static int
remote_read(RPMEMpool *rpp, void *buff)
{
	printf("restore Fibonacci numbers from the target...\n");
	if (rpmem_read(rpp, buff, FIBO_OFF, FIBO_SIZE, 0)) {
		printf("restore failed: %s\n", rpmem_errormsg());
		return 1;
	}
	return 0;
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

static struct pool_t *
map_pmem(const char *path, size_t *mapped_len)
{
	int is_pmem;
	struct pool_t *pool = pmem_map_file(path, 0, 0, 0, mapped_len,
			&is_pmem);
	if (!pool)
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

	return pool;
err:
	pmem_unmap(pool, *mapped_len);
	exit(1);
}

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
	struct pool_t *pool = map_pmem(path, &mapped_len);

	/* open remote pool */
	RPMEMpool *rpp = remote_create_or_open(target, poolset, pool, &created);
	if (!rpp) {
		ret = 1;
		goto unmap;
	}

	if (created) {
		/* zero remote copy */
		memset(&fibo_r, 0, FIBO_SIZE);
	} else {
		/* restore from remote */
		ret = remote_read(rpp, &fibo_r);
		if (ret) {
			/* invalidate remote copy */
			memset(&fibo_r, 1, FIBO_SIZE);
		}
	}

	/* validate copies from local and remote */
	ret = fibo_recovery(rpp, pool, &fibo_r, &initialized);
	if (ret) {
		fprintf(stderr, "recovery failed.\n");
		goto err;
	}

	/* generate new number */
	if (!initialized)
		fibo_generate(&pool->fibo);
	fibo_print(&pool->fibo);

	/* store to remote */
	ret = remote_write(rpp);
	if (ret)
		goto err;

	printf("rerun application to generate next Fibonacci number.\n");

err:
	/* close the remote pool */
	if (rpmem_close(rpp)) {
		fprintf(stderr, "rpmem_close: %s\n", rpmem_errormsg());
		exit(1);
	}

unmap:
	pmem_unmap(pool, mapped_len);
	return ret;
}
