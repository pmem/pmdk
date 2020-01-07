// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * daxio.c -- simple app for reading and writing data from/to
 *            Device DAX device using mmap instead of file I/O API
 */

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <limits.h>
#include <string.h>

#include <ndctl/libndctl.h>
#include <ndctl/libdaxctl.h>
#include <libpmem.h>

#include "util.h"
#include "os.h"
#include "os_dimm.h"

#define ALIGN_UP(size, align) (((size) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(size, align) ((size) & ~((align) - 1))

#define ERR(fmt, ...)\
do {\
	fprintf(stderr, "daxio: " fmt, ##__VA_ARGS__);\
} while (0)

#define FAIL(func)\
do {\
	fprintf(stderr, "daxio: %s:%d: %s: %s\n",\
		__func__, __LINE__, func, strerror(errno));\
} while (0)

#define USAGE_MESSAGE \
"Usage: daxio [option] ...\n"\
"Valid options:\n"\
"   -i, --input=FILE                - input device/file (default stdin)\n"\
"   -o, --output=FILE               - output device/file (default stdout)\n"\
"   -k, --skip=BYTES                - skip offset for input (default 0)\n"\
"   -s, --seek=BYTES                - seek offset for output (default 0)\n"\
"   -l, --len=BYTES                 - total length to perform the I/O\n"\
"   -b, --clear-bad-blocks=<yes|no> - clear bad blocks (default: yes)\n"\
"   -z, --zero                      - zeroing the device\n"\
"   -h. --help                      - print this help\n"\
"   -V, --version                   - display version of daxio\n"

struct daxio_device {
	char *path;
	int fd;
	size_t size;		/* actual file/device size */
	int is_devdax;

	/* Device DAX only */
	size_t align;		/* internal device alignment */
	char *addr;		/* mapping base address */
	size_t maplen;		/* mapping length */
	size_t offset;		/* seek or skip */

	unsigned major;
	unsigned minor;
	struct ndctl_ctx *ndctl_ctx;
	struct ndctl_region *region;	/* parent region */
};

/*
 * daxio_context -- context and arguments
 */
struct daxio_context {
	size_t len;	/* total length of I/O */
	int zero;
	int clear_bad_blocks;
	struct daxio_device src;
	struct daxio_device dst;
};

/*
 * default context
 */
static struct daxio_context Ctx = {
	SIZE_MAX,	/* len */
	0,		/* zero */
	1,		/* clear_bad_blocks */
	{ NULL, -1, SIZE_MAX, 0, 0, NULL, 0, 0, 0, 0, NULL, NULL },
	{ NULL, -1, SIZE_MAX, 0, 0, NULL, 0, 0, 0, 0, NULL, NULL },
};

/*
 * print_version -- print daxio version
 */
static void
print_version(void)
{
	printf("%s\n", SRCVERSION);
}

/*
 * print_usage -- print short description of usage
 */
static void
print_usage(void)
{
	fprintf(stderr, USAGE_MESSAGE);
}

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"input",			required_argument,	NULL,	'i'},
	{"output",			required_argument,	NULL,	'o'},
	{"skip",			required_argument,	NULL,	'k'},
	{"seek",			required_argument,	NULL,	's'},
	{"len",				required_argument,	NULL,	'l'},
	{"clear-bad-blocks",		required_argument,	NULL,	'b'},
	{"zero",			no_argument,		NULL,	'z'},
	{"help",			no_argument,		NULL,	'h'},
	{"version",			no_argument,		NULL,	'V'},
	{NULL,				0,			NULL,	 0 },
};

/*
 * parse_args -- (internal) parse command line arguments
 */
static int
parse_args(struct daxio_context *ctx, int argc, char * const argv[])
{
	int opt;
	size_t offset;
	size_t len;

	while ((opt = getopt_long(argc, argv, "i:o:k:s:l:b:zhV",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'i':
			ctx->src.path = optarg;
			break;
		case 'o':
			ctx->dst.path = optarg;
			break;
		case 'k':
			if (util_parse_size(optarg, &offset)) {
				ERR("'%s' -- invalid input offset\n", optarg);
				return -1;
			}
			ctx->src.offset = offset;
			break;
		case 's':
			if (util_parse_size(optarg, &offset)) {
				ERR("'%s' -- invalid output offset\n", optarg);
				return -1;
			}
			ctx->dst.offset = offset;
			break;
		case 'l':
			if (util_parse_size(optarg, &len)) {
				ERR("'%s' -- invalid length\n", optarg);
				return -1;
			}
			ctx->len = len;
			break;
		case 'z':
			ctx->zero = 1;
			break;
		case 'b':
			if (strcmp(optarg, "no") == 0) {
				ctx->clear_bad_blocks = 0;
			} else if (strcmp(optarg, "yes") == 0) {
				ctx->clear_bad_blocks = 1;
			} else {
				ERR(
					"'%s' -- invalid argument of the '--clear-bad-blocks' option\n",
					optarg);
				return -1;
			}
			break;
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
		case 'V':
			print_version();
			exit(EXIT_SUCCESS);
		default:
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	return 0;
}

/*
 * validate_args -- (internal) validate command line arguments
 */
static int
validate_args(struct daxio_context *ctx)
{
	if (ctx->zero && ctx->dst.path == NULL) {
		ERR("zeroing flag specified but no output file provided\n");
		return -1;
	}

	if (!ctx->zero && ctx->src.path == NULL && ctx->dst.path == NULL) {
		ERR("an input file and/or an output file must be provided\n");
		return -1;
	}

	/* if no input file provided, use stdin */
	if (ctx->src.path == NULL) {
		if (ctx->src.offset != 0) {
			ERR(
			"skip offset specified but no input file provided\n");
			return -1;
		}
		ctx->src.fd = STDIN_FILENO;
		ctx->src.path = "STDIN";
	}

	/* if no output file provided, use stdout */
	if (ctx->dst.path == NULL) {
		if (ctx->dst.offset != 0) {
			ERR(
			"seek offset specified but no output file provided\n");
			return -1;
		}
		ctx->dst.fd = STDOUT_FILENO;
		ctx->dst.path = "STDOUT";
	}

	return 0;
}

/*
 * match_dev_dax -- (internal) find Device DAX by major/minor device number
 */
static int
match_dev_dax(struct daxio_device *dev, struct daxctl_region *dax_region)
{
	struct daxctl_dev *d;

	daxctl_dev_foreach(dax_region, d) {
		if (dev->major == (unsigned)daxctl_dev_get_major(d) &&
		    dev->minor == (unsigned)daxctl_dev_get_minor(d)) {
			dev->size = daxctl_dev_get_size(d);
			return 1;
		}
	}

	return 0;
}

/*
 * find_dev_dax -- (internal) check if device is Device DAX
 *
 * If there is matching Device DAX, find its region, size and alignment.
 */
static int
find_dev_dax(struct ndctl_ctx *ndctl_ctx, struct daxio_device *dev)
{
	struct ndctl_bus *bus = NULL;
	struct ndctl_region *region = NULL;
	struct ndctl_dax *dax = NULL;
	struct daxctl_region *dax_region = NULL;

	ndctl_bus_foreach(ndctl_ctx, bus) {
		ndctl_region_foreach(bus, region) {
			ndctl_dax_foreach(region, dax) {
				dax_region = ndctl_dax_get_daxctl_region(dax);
				if (match_dev_dax(dev, dax_region)) {
					dev->is_devdax = 1;
					dev->align = ndctl_dax_get_align(dax);
					dev->region = region;
					return 1;
				}
			}
		}
	}

	/* try with dax regions */
	struct daxctl_ctx *daxctl_ctx;
	if (daxctl_new(&daxctl_ctx))
		return 0;

	int ret = 0;
	daxctl_region_foreach(daxctl_ctx, dax_region) {
		if (match_dev_dax(dev, dax_region)) {
			dev->is_devdax = 1;
			dev->align = daxctl_region_get_align(dax_region);
			dev->region = region;
			ret = 1;
			goto end;
		}
	}

end:
	daxctl_unref(daxctl_ctx);
	return ret;
}

/*
 * setup_device -- (internal) open/mmap file/device
 */
static int
setup_device(struct ndctl_ctx *ndctl_ctx, struct daxio_device *dev, int is_dst,
		int clear_bad_blocks)
{
	int ret;
	int flags = O_RDWR;
	int prot = is_dst ? PROT_WRITE : PROT_READ;

	if (dev->fd != -1) {
		dev->size = SIZE_MAX;
		return 0;	/* stdin/stdout */
	}

	/* try to open file/device (if exists) */
	dev->fd = os_open(dev->path, flags, S_IRUSR|S_IWUSR);
	if (dev->fd == -1) {
		ret = errno;
		if (ret == ENOENT && is_dst) {
			/* file does not exist - create it */
			flags = O_CREAT|O_WRONLY|O_TRUNC;
			dev->size = SIZE_MAX;
			dev->fd = os_open(dev->path, flags, S_IRUSR|S_IWUSR);
			if (dev->fd == -1) {
				FAIL("open");
				return -1;
			}
			return 0;
		} else {
			ERR("failed to open '%s': %s\n", dev->path,
				strerror(errno));
			return -1;
		}
	}

	struct stat stbuf;
	ret = fstat(dev->fd, &stbuf);
	if (ret == -1) {
		FAIL("stat");
		return -1;
	}

	/* check if this is regular file or device */
	if (S_ISREG(stbuf.st_mode)) {
		if (is_dst)
			dev->size = SIZE_MAX;
		else
			dev->size = (size_t)stbuf.st_size;
	} else if (S_ISBLK(stbuf.st_mode)) {
		dev->size = (size_t)stbuf.st_size;
	} else if (S_ISCHR(stbuf.st_mode)) {
		dev->size = SIZE_MAX;
		dev->major = major(stbuf.st_rdev);
		dev->minor = minor(stbuf.st_rdev);
	} else {
		return -1;
	}

	/* check if this is Device DAX */
	if (S_ISCHR(stbuf.st_mode))
		find_dev_dax(ndctl_ctx, dev);

	if (!dev->is_devdax)
		return 0;

	if (is_dst && clear_bad_blocks) {
		/* XXX - clear only badblocks in range bound by offset/len */
		if (os_dimm_devdax_clear_badblocks_all(dev->path)) {
			ERR("failed to clear bad blocks on \"%s\"\n"
			    "       Probably you have not enough permissions to do that.\n"
			    "       You can choose one of three options now:\n"
			    "       1) run 'daxio' with 'sudo' or as 'root',\n"
			    "       2) turn off clearing bad blocks using\n"
			    "          the '-b/--clear-bad-blocks=no' option or\n"
			    "       3) change permissions of some resource files -\n"
			    "          - for details see the description of the CHECK_BAD_BLOCKS\n"
			    "          compat feature in the pmempool-feature(1) man page.\n",
				dev->path);
			return -1;
		}
	}

	if (dev->align == ULONG_MAX) {
		ERR("cannot determine device alignment for \"%s\"\n",
				dev->path);
		return -1;
	}

	if (dev->offset > dev->size) {
		ERR("'%zu' -- offset beyond device size (%zu)\n",
				dev->offset, dev->size);
		return -1;
	}

	/* align len/offset to the internal device alignment */
	dev->maplen = ALIGN_UP(dev->size, dev->align);
	size_t offset = ALIGN_DOWN(dev->offset, dev->align);
	dev->offset = dev->offset - offset;
	dev->maplen = dev->maplen - offset;

	dev->addr = mmap(NULL, dev->maplen, prot, MAP_SHARED, dev->fd,
			(off_t)offset);
	if (dev->addr == MAP_FAILED) {
		FAIL("mmap");
		return -1;
	}

	return 0;
}

/*
 * setup_devices -- (internal) open/mmap input and output
 */
static int
setup_devices(struct ndctl_ctx *ndctl_ctx, struct daxio_context *ctx)
{
	if (!ctx->zero &&
	    setup_device(ndctl_ctx, &ctx->src, 0, ctx->clear_bad_blocks))
		return -1;
	return setup_device(ndctl_ctx, &ctx->dst, 1, ctx->clear_bad_blocks);
}

/*
 * adjust_io_len -- (internal) calculate I/O length if not specified
 */
static void
adjust_io_len(struct daxio_context *ctx)
{
	size_t src_len = ctx->src.maplen - ctx->src.offset;
	size_t dst_len = ctx->dst.maplen - ctx->dst.offset;
	size_t max_len = SIZE_MAX;

	if (ctx->zero)
		assert(ctx->dst.is_devdax);
	else
		assert(ctx->src.is_devdax || ctx->dst.is_devdax);

	if (ctx->src.is_devdax)
		max_len = src_len;
	if (ctx->dst.is_devdax)
		max_len = max_len < dst_len ? max_len : dst_len;

	/* if length is specified and is not bigger than mmapped region */
	if (ctx->len != SIZE_MAX && ctx->len <= max_len)
		return;

	/* adjust len to device size */
	ctx->len = max_len;
}

/*
 * cleanup_device -- (internal) unmap/close file/device
 */
static void
cleanup_device(struct daxio_device *dev)
{
	if (dev->addr)
		(void) munmap(dev->addr, dev->maplen);
	if (dev->path && dev->fd != -1)
		(void) close(dev->fd);
}

/*
 * cleanup_devices -- (internal) unmap/close input and output
 */
static void
cleanup_devices(struct daxio_context *ctx)
{
	cleanup_device(&ctx->dst);
	if (!ctx->zero)
		cleanup_device(&ctx->src);
}

/*
 * do_io -- (internal) write data to device/file
 */
static int
do_io(struct ndctl_ctx *ndctl_ctx, struct daxio_context *ctx)
{
	ssize_t cnt = 0;

	assert(ctx->src.is_devdax || ctx->dst.is_devdax);

	if (ctx->zero) {
		if (ctx->dst.offset > ctx->dst.maplen) {
			ERR("output offset larger than device size");
			return -1;
		}
		if (ctx->dst.offset + ctx->len > ctx->dst.maplen) {
			ERR("output offset beyond device size");
			return -1;
		}

		char *dst_addr = ctx->dst.addr + ctx->dst.offset;
		pmem_memset_persist(dst_addr, 0, ctx->len);
		cnt = (ssize_t)ctx->len;
	} else if (ctx->src.is_devdax && ctx->dst.is_devdax) {
		/* memcpy between src and dst */
		char *src_addr = ctx->src.addr + ctx->src.offset;
		char *dst_addr = ctx->dst.addr + ctx->dst.offset;
		pmem_memcpy_persist(dst_addr, src_addr, ctx->len);
		cnt = (ssize_t)ctx->len;
	} else if (ctx->src.is_devdax) {
		/* write to file directly from mmap'ed src */
		char *src_addr = ctx->src.addr + ctx->src.offset;
		if (ctx->dst.offset) {
			if (lseek(ctx->dst.fd, (off_t)ctx->dst.offset,
					SEEK_SET) < 0) {
				FAIL("lseek");
				goto err;
			}
		}
		do {
			ssize_t wcnt = write(ctx->dst.fd, src_addr + cnt,
					ctx->len - (size_t)cnt);
			if (wcnt == -1) {
				FAIL("write");
				goto err;
			}
			cnt += wcnt;
		} while ((size_t)cnt < ctx->len);
	} else if (ctx->dst.is_devdax) {
		/* read from file directly to mmap'ed dst */
		char *dst_addr = ctx->dst.addr + ctx->dst.offset;
		if (ctx->src.offset) {
			if (lseek(ctx->src.fd, (off_t)ctx->src.offset,
					SEEK_SET) < 0) {
				FAIL("lseek");
				return -1;
			}
		}
		do {
			ssize_t rcnt = read(ctx->src.fd, dst_addr + cnt,
					ctx->len - (size_t)cnt);
			if (rcnt == -1) {
				FAIL("read");
				goto err;
			}
			/* end of file */
			if (rcnt == 0)
				break;
			cnt = cnt + rcnt;
		} while ((size_t)cnt < ctx->len);

		pmem_persist(dst_addr, (size_t)cnt);

		if ((size_t)cnt != ctx->len)
			ERR("requested size %zu larger than source\n",
					ctx->len);
	}

	ERR("copied %zd bytes to device \"%s\"\n", cnt, ctx->dst.path);
	return 0;

err:
	ERR("failed to perform I/O\n");
	return -1;
}

int
main(int argc, char **argv)
{
	struct ndctl_ctx *ndctl_ctx;
	int ret = EXIT_SUCCESS;

	if (parse_args(&Ctx, argc, argv))
		return EXIT_FAILURE;

	if (validate_args(&Ctx))
		return EXIT_FAILURE;

	if (ndctl_new(&ndctl_ctx))
		return EXIT_FAILURE;

	if (setup_devices(ndctl_ctx, &Ctx)) {
		ret = EXIT_FAILURE;
		goto err;
	}

	if (!Ctx.src.is_devdax && !Ctx.dst.is_devdax) {
		ERR("neither input nor output is device dax\n");
		ret = EXIT_FAILURE;
		goto err;
	}

	adjust_io_len(&Ctx);

	if (do_io(ndctl_ctx, &Ctx))
		ret = EXIT_FAILURE;

err:
	cleanup_devices(&Ctx);
	ndctl_unref(ndctl_ctx);

	return ret;
}
