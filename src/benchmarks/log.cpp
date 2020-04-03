// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */
/*
 * log.cpp -- pmemlog benchmarks definitions
 */

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

#include "benchmark.hpp"
#include "file.h"
#include "libpmemlog.h"
#include "os.h"
#include "poolset_util.hpp"
#include "rand.h"

/*
 * Size of pool header, pool descriptor
 * and additional page alignment overhead
 */
#define POOL_HDR_SIZE (3 * 4096)
#define MIN_VEC_SIZE 1

/*
 * prog_args - benchmark's specific command line arguments
 */
struct prog_args {
	unsigned seed;	 /* seed for pseudo-random generator */
	bool rand;	 /* use random numbers */
	int vec_size;	 /* vector size */
	size_t el_size;	 /* size of single append */
	size_t min_size; /* minimum size for random mode */
	bool no_warmup;	 /* don't do warmup */
	bool fileio;	 /* use file io instead of pmemlog */
};

/*
 * thread_info - thread specific data
 */
struct log_worker_info {
	rng_t rng;
	struct iovec *iov; /* io vector */
	char *buf;	   /* buffer for write/read operations */
	size_t buf_size;   /* buffer size */
	size_t buf_ptr;	   /* pointer for read operations */
	size_t *rand_sizes;
	size_t *vec_sizes; /* sum of sizes in vector */
};

/*
 * log_bench - main context of benchmark
 */
struct log_bench {
	size_t psize;		/* size of pool */
	PMEMlogpool *plp;	/* pmemlog handle */
	struct prog_args *args; /* benchmark specific arguments */
	int fd;			/* file descriptor for file io mode */
	rng_t rng;
	/*
	 * Pointer to the main benchmark operation. The appropriate function
	 * will be assigned depending on the benchmark specific arguments.
	 */
	int (*func_op)(struct benchmark *, struct operation_info *);
};

/*
 * do_warmup -- do warmup by writing the whole pool area
 */
static int
do_warmup(struct log_bench *lb, size_t nops)
{
	int ret = 0;
	size_t bsize = lb->args->vec_size * lb->args->el_size;
	auto *buf = (char *)calloc(1, bsize);
	if (!buf) {
		perror("calloc");
		return -1;
	}

	if (!lb->args->fileio) {
		for (size_t i = 0; i < nops; i++) {
			if (pmemlog_append(lb->plp, buf, lb->args->el_size) <
			    0) {
				ret = -1;
				perror("pmemlog_append");
				goto out;
			}
		}

		pmemlog_rewind(lb->plp);

	} else {
		for (size_t i = 0; i < nops; i++) {
			if (write(lb->fd, buf, (unsigned)lb->args->el_size) !=
			    (ssize_t)lb->args->el_size) {
				ret = -1;
				perror("write");
				os_close(lb->fd);
				goto out;
			}
		}

		if (os_lseek(lb->fd, 0, SEEK_SET) < 0) {
			ret = -1;
			perror("lseek");
			os_close(lb->fd);
		}
	}

out:
	free(buf);

	return ret;
}

/*
 * log_append -- performs pmemlog_append operation
 */
static int
log_append(struct benchmark *bench, struct operation_info *info)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	assert(lb);

	auto *worker_info = (struct log_worker_info *)info->worker->priv;

	assert(worker_info);

	size_t size = lb->args->rand ? worker_info->rand_sizes[info->index]
				     : lb->args->el_size;

	if (pmemlog_append(lb->plp, worker_info->buf, size) < 0) {
		perror("pmemlog_append");
		return -1;
	}

	return 0;
}

/*
 * log_appendv -- performs pmemlog_appendv operation
 */
static int
log_appendv(struct benchmark *bench, struct operation_info *info)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	assert(lb);

	auto *worker_info = (struct log_worker_info *)info->worker->priv;

	assert(worker_info);

	struct iovec *iov = &worker_info->iov[info->index * lb->args->vec_size];

	if (pmemlog_appendv(lb->plp, iov, lb->args->vec_size) < 0) {
		perror("pmemlog_appendv");
		return -1;
	}

	return 0;
}

/*
 * fileio_append -- performs fileio append operation
 */
static int
fileio_append(struct benchmark *bench, struct operation_info *info)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	assert(lb);

	auto *worker_info = (struct log_worker_info *)info->worker->priv;

	assert(worker_info);

	size_t size = lb->args->rand ? worker_info->rand_sizes[info->index]
				     : lb->args->el_size;

	if (write(lb->fd, worker_info->buf, (unsigned)size) != (ssize_t)size) {
		perror("write");
		return -1;
	}

	return 0;
}

/*
 * fileio_appendv -- performs fileio appendv operation
 */
static int
fileio_appendv(struct benchmark *bench, struct operation_info *info)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	assert(lb != nullptr);

	auto *worker_info = (struct log_worker_info *)info->worker->priv;

	assert(worker_info);

	struct iovec *iov = &worker_info->iov[info->index * lb->args->vec_size];
	size_t vec_size = worker_info->vec_sizes[info->index];

	if (os_writev(lb->fd, iov, lb->args->vec_size) != (ssize_t)vec_size) {
		perror("writev");
		return -1;
	}

	return 0;
}

/*
 * log_process_data -- callback function for pmemlog_walk.
 */
static int
log_process_data(const void *buf, size_t len, void *arg)
{
	auto *worker_info = (struct log_worker_info *)arg;
	size_t left = worker_info->buf_size - worker_info->buf_ptr;
	if (len > left) {
		worker_info->buf_ptr = 0;
		left = worker_info->buf_size;
	}

	len = len < left ? len : left;
	assert(len <= left);

	void *buff = &worker_info->buf[worker_info->buf_ptr];
	memcpy(buff, buf, len);
	worker_info->buf_ptr += len;

	return 1;
}

/*
 * fileio_read -- perform single fileio read
 */
static int
fileio_read(int fd, ssize_t len, struct log_worker_info *worker_info)
{
	ssize_t left = worker_info->buf_size - worker_info->buf_ptr;
	if (len > left) {
		worker_info->buf_ptr = 0;
		left = worker_info->buf_size;
	}

	len = len < left ? len : left;
	assert(len <= left);

	size_t off = worker_info->buf_ptr;
	void *buff = &worker_info->buf[off];
	if ((len = pread(fd, buff, len, off)) < 0)
		return -1;

	worker_info->buf_ptr += len;

	return 1;
}

/*
 * log_read_op -- perform read operation
 */
static int
log_read_op(struct benchmark *bench, struct operation_info *info)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	assert(lb);

	auto *worker_info = (struct log_worker_info *)info->worker->priv;

	assert(worker_info);

	worker_info->buf_ptr = 0;

	size_t chunk_size = lb->args->rand
		? worker_info->rand_sizes[info->index]
		: lb->args->el_size;

	if (!lb->args->fileio) {
		pmemlog_walk(lb->plp, chunk_size, log_process_data,
			     worker_info);
		return 0;
	}

	int ret;
	while ((ret = fileio_read(lb->fd, chunk_size, worker_info)) == 1)
		;

	return ret;
}

/*
 * log_init_worker -- init benchmark worker
 */
static int
log_init_worker(struct benchmark *bench, struct benchmark_args *args,
		struct worker_info *worker)
{
	int ret = 0;
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);
	size_t i_size, n_vectors;
	assert(lb);

	auto *worker_info = (struct log_worker_info *)malloc(
		sizeof(struct log_worker_info));
	if (!worker_info) {
		perror("malloc");
		return -1;
	}

	/* allocate buffer for append / read */
	worker_info->buf_size = lb->args->el_size * lb->args->vec_size;
	worker_info->buf = (char *)malloc(worker_info->buf_size);
	if (!worker_info->buf) {
		perror("malloc");
		ret = -1;
		goto err_free_worker_info;
	}

	/*
	 * For random mode, each operation has its own vector with
	 * random sizes. Otherwise there is only one vector with
	 * equal sizes.
	 */
	n_vectors = args->n_ops_per_thread;
	worker_info->iov = (struct iovec *)malloc(
		n_vectors * lb->args->vec_size * sizeof(struct iovec));
	if (!worker_info->iov) {
		perror("malloc");
		ret = -1;
		goto err_free_buf;
	}

	if (lb->args->rand) {
		/* each thread has random seed */
		randomize_r(&worker_info->rng, rnd64_r(&lb->rng));

		/* each vector element has its own random size */
		size_t n_sizes = args->n_ops_per_thread * lb->args->vec_size;
		worker_info->rand_sizes = (size_t *)malloc(
			n_sizes * sizeof(*worker_info->rand_sizes));
		if (!worker_info->rand_sizes) {
			perror("malloc");
			ret = -1;
			goto err_free_iov;
		}

		/* generate append sizes */
		for (size_t i = 0; i < n_sizes; i++) {
			size_t width = lb->args->el_size - lb->args->min_size;
			worker_info->rand_sizes[i] =
				rnd64_r(&worker_info->rng) % width +
				lb->args->min_size;
		}
	} else {
		worker_info->rand_sizes = nullptr;
	}

	worker_info->vec_sizes = (size_t *)calloc(
		args->n_ops_per_thread, sizeof(*worker_info->vec_sizes));
	if (!worker_info->vec_sizes) {
		perror("malloc\n");
		ret = -1;
		goto err_free_rand_sizes;
	}

	/* fill up the io vectors */
	i_size = 0;
	for (size_t n = 0; n < args->n_ops_per_thread; n++) {
		size_t buf_ptr = 0;
		size_t vec_off = n * lb->args->vec_size;
		for (int i = 0; i < lb->args->vec_size; ++i) {
			size_t el_size = lb->args->rand
				? worker_info->rand_sizes[i_size++]
				: lb->args->el_size;

			worker_info->iov[vec_off + i].iov_base =
				&worker_info->buf[buf_ptr];
			worker_info->iov[vec_off + i].iov_len = el_size;

			worker_info->vec_sizes[n] += el_size;

			buf_ptr += el_size;
		}
	}

	worker->priv = worker_info;

	return 0;
err_free_rand_sizes:
	free(worker_info->rand_sizes);
err_free_iov:
	free(worker_info->iov);
err_free_buf:
	free(worker_info->buf);
err_free_worker_info:
	free(worker_info);

	return ret;
}

/*
 * log_free_worker -- cleanup benchmark worker
 */
static void
log_free_worker(struct benchmark *bench, struct benchmark_args *args,
		struct worker_info *worker)
{

	auto *worker_info = (struct log_worker_info *)worker->priv;
	assert(worker_info);

	free(worker_info->buf);
	free(worker_info->iov);
	free(worker_info->rand_sizes);
	free(worker_info->vec_sizes);
	free(worker_info);
}

/*
 * log_init -- benchmark initialization function
 */
static int
log_init(struct benchmark *bench, struct benchmark_args *args)
{
	int ret = 0;
	assert(bench);
	assert(args != nullptr);
	assert(args->opts != nullptr);
	struct benchmark_info *bench_info;

	char path[PATH_MAX];
	if (util_safe_strcpy(path, args->fname, sizeof(path)) != 0)
		return -1;

	enum file_type type = util_file_get_type(args->fname);
	if (type == OTHER_ERROR) {
		fprintf(stderr, "could not check type of file %s\n",
			args->fname);
		return -1;
	}

	auto *lb = (struct log_bench *)malloc(sizeof(struct log_bench));

	if (!lb) {
		perror("malloc");
		return -1;
	}

	lb->args = (struct prog_args *)args->opts;
	lb->args->el_size = args->dsize;

	if (lb->args->vec_size == 0)
		lb->args->vec_size = 1;

	if (lb->args->rand && lb->args->min_size > lb->args->el_size) {
		errno = EINVAL;
		ret = -1;
		goto err_free_lb;
	}

	if (lb->args->rand && lb->args->min_size == lb->args->el_size)
		lb->args->rand = false;

	randomize_r(&lb->rng, lb->args->seed);

	/* align pool size to ensure that we have enough usable space */
	lb->psize =
		ALIGN_UP(POOL_HDR_SIZE +
				 args->n_ops_per_thread * args->n_threads *
					 lb->args->vec_size * lb->args->el_size,
			 Mmap_align);

	/* calculate a required pool size */
	if (lb->psize < PMEMLOG_MIN_POOL)
		lb->psize = PMEMLOG_MIN_POOL;

	if (args->is_poolset || type == TYPE_DEVDAX) {
		if (lb->args->fileio) {
			fprintf(stderr,
				"fileio not supported on device dax nor poolset\n");
			ret = -1;
			goto err_free_lb;
		}

		if (args->fsize < lb->psize) {
			fprintf(stderr, "file size too large\n");
			ret = -1;
			goto err_free_lb;
		}

		lb->psize = 0;
	} else if (args->is_dynamic_poolset) {
		if (lb->args->fileio) {
			fprintf(stderr,
				"fileio not supported with dynamic poolset\n");
			ret = -1;
			goto err_free_lb;
		}

		ret = dynamic_poolset_create(args->fname, lb->psize);
		if (ret == -1)
			goto err_free_lb;

		if (util_safe_strcpy(path, POOLSET_PATH, sizeof(path)) != 0)
			goto err_free_lb;

		lb->psize = 0;
	}

	bench_info = pmembench_get_info(bench);

	if (!lb->args->fileio) {
		if ((lb->plp = pmemlog_create(path, lb->psize, args->fmode)) ==
		    nullptr) {
			perror("pmemlog_create");
			ret = -1;
			goto err_free_lb;
		}

		bench_info->operation =
			(lb->args->vec_size > 1) ? log_appendv : log_append;
	} else {
		int flags = O_CREAT | O_RDWR | O_SYNC;

		/* Create a file if it does not exist. */
		if ((lb->fd = os_open(args->fname, flags, args->fmode)) < 0) {
			perror(args->fname);
			ret = -1;
			goto err_free_lb;
		}

		/* allocate the pmem */
		if ((errno = os_posix_fallocate(lb->fd, 0, lb->psize)) != 0) {
			perror("posix_fallocate");
			ret = -1;
			goto err_close;
		}
		bench_info->operation = (lb->args->vec_size > 1)
			? fileio_appendv
			: fileio_append;
	}

	if (!lb->args->no_warmup && type != TYPE_DEVDAX) {
		size_t warmup_nops = args->n_threads * args->n_ops_per_thread;
		if (do_warmup(lb, warmup_nops)) {
			fprintf(stderr, "warmup failed\n");
			ret = -1;
			goto err_close;
		}
	}

	pmembench_set_priv(bench, lb);

	return 0;

err_close:
	if (lb->args->fileio)
		os_close(lb->fd);
	else
		pmemlog_close(lb->plp);
err_free_lb:
	free(lb);

	return ret;
}

/*
 * log_exit -- cleanup benchmark
 */
static int
log_exit(struct benchmark *bench, struct benchmark_args *args)
{
	auto *lb = (struct log_bench *)pmembench_get_priv(bench);

	if (!lb->args->fileio)
		pmemlog_close(lb->plp);
	else
		os_close(lb->fd);

	free(lb);

	return 0;
}

/* command line options definition */
static struct benchmark_clo log_clo[6];

/* log_append benchmark info */
static struct benchmark_info log_append_info;

/* log_read benchmark info */
static struct benchmark_info log_read_info;

CONSTRUCTOR(log_constructor)
void
log_constructor(void)
{
	log_clo[0].opt_short = 'r';
	log_clo[0].opt_long = "random";
	log_clo[0].descr = "Use random sizes for append/read";
	log_clo[0].off = clo_field_offset(struct prog_args, rand);
	log_clo[0].type = CLO_TYPE_FLAG;

	log_clo[1].opt_short = 'S';
	log_clo[1].opt_long = "seed";
	log_clo[1].descr = "Random mode";
	log_clo[1].off = clo_field_offset(struct prog_args, seed);
	log_clo[1].def = "1";
	log_clo[1].type = CLO_TYPE_UINT;
	log_clo[1].type_uint.size = clo_field_size(struct prog_args, seed);
	log_clo[1].type_uint.base = CLO_INT_BASE_DEC;
	log_clo[1].type_uint.min = 1;
	log_clo[1].type_uint.max = UINT_MAX;

	log_clo[2].opt_short = 'i';
	log_clo[2].opt_long = "file-io";
	log_clo[2].descr = "File I/O mode";
	log_clo[2].off = clo_field_offset(struct prog_args, fileio);
	log_clo[2].type = CLO_TYPE_FLAG;

	log_clo[3].opt_short = 'w';
	log_clo[3].opt_long = "no-warmup";
	log_clo[3].descr = "Don't do warmup", log_clo[3].type = CLO_TYPE_FLAG;
	log_clo[3].off = clo_field_offset(struct prog_args, no_warmup);

	log_clo[4].opt_short = 'm';
	log_clo[4].opt_long = "min-size";
	log_clo[4].descr = "Minimum size of append/read for "
			   "random mode";
	log_clo[4].type = CLO_TYPE_UINT;
	log_clo[4].off = clo_field_offset(struct prog_args, min_size);
	log_clo[4].def = "1";
	log_clo[4].type_uint.size = clo_field_size(struct prog_args, min_size);
	log_clo[4].type_uint.base = CLO_INT_BASE_DEC;
	log_clo[4].type_uint.min = 1;
	log_clo[4].type_uint.max = UINT64_MAX;

	/* this one is only for log_append */
	log_clo[5].opt_short = 'v';
	log_clo[5].opt_long = "vector";
	log_clo[5].descr = "Vector size";
	log_clo[5].off = clo_field_offset(struct prog_args, vec_size);
	log_clo[5].def = "1";
	log_clo[5].type = CLO_TYPE_INT;
	log_clo[5].type_int.size = clo_field_size(struct prog_args, vec_size);
	log_clo[5].type_int.base = CLO_INT_BASE_DEC;
	log_clo[5].type_int.min = MIN_VEC_SIZE;
	log_clo[5].type_int.max = INT_MAX;

	log_append_info.name = "log_append";
	log_append_info.brief = "Benchmark for pmemlog_append() "
				"operation";
	log_append_info.init = log_init;
	log_append_info.exit = log_exit;
	log_append_info.multithread = true;
	log_append_info.multiops = true;
	log_append_info.init_worker = log_init_worker;
	log_append_info.free_worker = log_free_worker;
	/* this will be assigned in log_init */
	log_append_info.operation = nullptr;
	log_append_info.measure_time = true;
	log_append_info.clos = log_clo;
	log_append_info.nclos = ARRAY_SIZE(log_clo);
	log_append_info.opts_size = sizeof(struct prog_args);
	log_append_info.rm_file = true;
	log_append_info.allow_poolset = true;
	REGISTER_BENCHMARK(log_append_info);

	log_read_info.name = "log_read";
	log_read_info.brief = "Benchmark for pmemlog_walk() "
			      "operation";
	log_read_info.init = log_init;
	log_read_info.exit = log_exit;
	log_read_info.multithread = true;
	log_read_info.multiops = true;
	log_read_info.init_worker = log_init_worker;
	log_read_info.free_worker = log_free_worker;
	log_read_info.operation = log_read_op;
	log_read_info.measure_time = true;
	log_read_info.clos = log_clo;
	/* without vector */
	log_read_info.nclos = ARRAY_SIZE(log_clo) - 1;
	log_read_info.opts_size = sizeof(struct prog_args);
	log_read_info.rm_file = true;
	log_read_info.allow_poolset = true;
	REGISTER_BENCHMARK(log_read_info);
};
