/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * info.c -- pmempool info command main source file
 */

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <err.h>
#include <inttypes.h>
#define	__USE_UNIX98
#include <unistd.h>
#include "common.h"
#include "output.h"

/*
 * Verbose levels used in application:
 *
 * VERBOSE_DEFAULT:
 * Default value for application's verbosity level.
 * This is also set for data structures which should be
 * printed without any command line argument.
 *
 * VERBOSE_MAX:
 * Maximum value for application's verbosity level.
 * This value is used when -v command line argument passed.
 *
 * VERBOSE_SILENT:
 * This value is higher than VERBOSE_MAX and it is used only
 * for verbosity levels of data structures which should _not_ be
 * printed without specified command line arguments.
 */
#define	VERBOSE_DEFAULT	1
#define	VERBOSE_MAX	2
#define	VERBOSE_SILENT	3

/*
 * pmempool_info_args -- structure for storing command line arguments
 */
struct pmempool_info_args {
	char **files;		/* table of input files */
	unsigned int n_files;	/* number of input files */
	unsigned int col_width;	/* column width for printing fields */
	bool human;		/* sizes in human-readable formats */
	bool force;		/* force parsing pool */
	pmem_pool_type_t type;	/* forced pool type */
	bool skip_zeros;	/* skip blocks marked with zero flag */
	bool skip_error;	/* skip blocks marked with error flag */
	bool skip_no_flag;	/* skip blocks not marked with any flag */
	bool use_range;		/* use range for blocks */
	struct ranges ranges;	/* range of block/chunks to dump */
	struct range entire;	/* entire range of data */
	size_t walk;		/* data chunk size */
	int vlevel;		/* verbosity level */
	int vdata;		/* verbosity level for data dump */
	int vmap;		/* verbosity level for BTT Map */
	int vflog;		/* verbosity level for BTT FLOG */
	int vbackup;		/* verbosity level for BTT Info backup */
	int vhdrdump;		/* verbosity level for headers hexdump */
	int vstats;		/* verbosity level for statistics */
};

/*
 * Default arguments
 */
static const struct pmempool_info_args pmempool_info_args_default = {
	.files		= NULL,
	.n_files	= 0,
	/*
	 * Picked experimentally based on used fields names.
	 * This should be at least the number of characters of
	 * the longest field name.
	 */
	.col_width	= 24,
	.human		= false,
	.force		= false,
	.type		= PMEM_POOL_TYPE_NONE,
	.skip_zeros	= false,
	.skip_error	= false,
	.skip_no_flag	= false,
	.use_range	= false,
	.entire		= {
		.first	= 0,
		.last	= UINT64_MAX,
	},
	.walk		= 0,
	.vlevel		= VERBOSE_DEFAULT,
	.vdata		= VERBOSE_SILENT,
	.vmap		= VERBOSE_SILENT,
	.vflog		= VERBOSE_SILENT,
	.vbackup	= VERBOSE_SILENT,
	.vhdrdump	= VERBOSE_SILENT,
	.vstats		= VERBOSE_SILENT,
};

/*
 * pmem_info -- context for pmeminfo application
 */
struct pmem_info {
	const char *file_name;	/* current file name */
	int fd;			/* file descriptor */
	struct pmempool_info_args args;	/* arguments parsed from command line */
};

/*
 * pmem_blk_stats -- structure for holding statistics for pmemblk
 */
struct pmem_blk_stats {
	uint32_t total;		/* number of processed blocks */
	uint32_t zeros;		/* number of blocks marked by zero flag */
	uint32_t errors;	/* number of blocks marked by error flag */
	uint32_t noflag;	/* number of blocks not marked with any flag */
};

/*
 * long-options -- structure holding long options.
 */
static const struct option long_options[] = {
	{"version",	no_argument,		0,	'V'},
	{"help",	no_argument,		0,	'?'},
	{"human",	no_argument,		0,	'h'},
	{"force",	required_argument,	0,	'f'},
	{"skip-zeros",	no_argument,		0,	'z'},
	{"skip-error",	no_argument,		0,	'e'},
	{"skip-no-flag", no_argument,		0,	'u'},
	{"range",	required_argument,	0,	'r'},
	{"data",	no_argument,		0,	'd'},
	{"map",		no_argument,		0,	'm'},
	{"flog",	no_argument,		0,	'g'},
	{"backup",	no_argument,		0,	'B'},
	{"headers-hex",	no_argument,		0,	'x'},
	{"walk",	required_argument,	0,	'w'},
	{"stats",	no_argument,		0,	's'},
	{0,		0,			0,	 0 },
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Show information about pmem pool from specified file(s).\n"
"\n"
"Common options:\n"
"  -f, --force blk|log  force parsing a pool of specified type\n"
"  -h, --human          print sizes in human readable format (e.g. 2M, 1T)\n"
"  -x, --headers-hex    hexdump all headers\n"
"  -d, --data           dump log data and blocks\n"
"  -s, --stats          print statistics\n"
"  -r, --range <range>  range of blocks/chunks to dump\n"
"  -V, --version        display version and exit\n"
"  -?, --help           display this help and exit\n"
"\n"
"Options for PMEMLOG:\n"
"  -w, --walk <size>    chunk size\n"
"\n"
"Options for PMEMBLK:\n"
"  -m, --map            print BTT Map entries\n"
"  -g, --flog           print BTT FLOG entries\n"
"  -B, --backup         print BTT Info header backup\n"
"  -z, --skip-zeros     skip blocks marked with zero flag\n"
"  -e, --skip-error     skip blocks marked with error flag\n"
"  -u, --skip-no-flag   skip blocks not marked with any flag\n"
"\n"
"For complete documentation see %s-info(1) manual page.\n"
;

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s info [<args>] <file>..\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_info_help -- print application usage detailed description
 */
void
pmempool_info_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * parse_args -- parse command line arguments
 *
 * Parse command line arguments and store them in pmempool_info_args
 * structure.
 * Terminates process if invalid arguments passed.
 */
static int
parse_args(char *appname, int argc, char *argv[],
		struct pmempool_info_args *argsp)
{
	int opt;
	int option_index;
	if (argc == 1) {
		print_usage(appname);

		return -1;
	}

	while ((opt = getopt_long(argc, argv, "hf:ezuF:L:c:dmxV?w:gBsr:",
				long_options, &option_index)) != -1) {
		switch (opt) {
		case 'V':
			print_version(appname);
			exit(EXIT_SUCCESS);
		case '?':
			if (optopt == '\0') {
				pmempool_info_help(appname);
				exit(EXIT_SUCCESS);
			}
			print_usage(appname);
			exit(EXIT_FAILURE);
		case 'h':
			argsp->human = true;
			break;
		case 'f':
			argsp->type = pmem_pool_type_parse_str(optarg);
			if (argsp->type == PMEM_POOL_TYPE_UNKNOWN) {
				out_err("'%s' -- unknown pool type\n", optarg);
				return -1;
			}
			argsp->force = true;
			break;
		case 'e':
			argsp->skip_error = true;
			break;
		case 'z':
			argsp->skip_zeros = true;
			break;
		case 'u':
			argsp->skip_no_flag = true;
			break;
		case 'r':
			if (util_parse_ranges(optarg, &argsp->ranges,
						&argsp->entire)) {
				out_err("'%s' -- cannot parse\n",
						optarg);
				return -1;
			}
			argsp->use_range = true;
			break;
		case 'd':
			argsp->vdata = VERBOSE_DEFAULT;
			break;
		case 'm':
			argsp->vmap = VERBOSE_DEFAULT;
			break;
		case 'g':
			argsp->vflog = VERBOSE_DEFAULT;
			break;
		case 'B':
			argsp->vbackup = VERBOSE_DEFAULT;
			break;
		case 'x':
			argsp->vhdrdump = VERBOSE_DEFAULT;
			break;
		case 's':
			argsp->vstats = VERBOSE_DEFAULT;
			break;
		case 'w':
			argsp->walk = (size_t)atoll(optarg);
			if (argsp->walk == 0) {
				out_err("'%s' -- invalid chunk size\n",
					optarg);
				return -1;
			}
			break;
		default:
			print_usage(appname);
			return -1;
		}
	}

	/* store pointer to files list */
	if (optind < argc) {
		argsp->n_files = argc - optind;
		argsp->files = &argv[optind];
	} else {
		argsp->n_files = 0;
		argsp->files = NULL;
	}

	if (!argsp->use_range)
		util_ranges_add(&argsp->ranges, argsp->entire.first,
				argsp->entire.last);

	return 0;
}

/*
 * pmem_blk_stats_reset -- reset statistics
 */
static void
pmem_blk_stats_reset(struct pmem_blk_stats *statsp)
{
	memset(statsp, 0, sizeof (*statsp));
}

/*
 * pmempool_info_read -- read data from file
 */
static int
pmempool_info_read(struct pmem_info *pip, void *buff, size_t nbytes, off_t off)
{
	ssize_t ret = pread(pip->fd, buff, nbytes, off);
	if (ret < 0)
		warn("%s", pip->file_name);
	return !(nbytes == ret);
}

/*
 * pmempool_info_get_range -- get blocks/data chunk range
 *
 * Get range based on command line arguments and maximum value.
 * Return value:
 * 0 - range is empty
 * 1 - range is not empty
 */
static int
pmempool_info_get_range(struct pmem_info *pip, struct range *rangep,
		struct range *curp, uint32_t max, off_t offset)
{
	/* not using range */
	if (!pip->args.use_range) {
		rangep->first = 0;
		rangep->last = max;

		return 1;
	}

	if (curp->first > offset + max)
		return 0;

	if (curp->first >= offset)
		rangep->first = curp->first - offset;
	else
		rangep->first = 0;

	if (curp->last < offset)
		return 0;

	if (curp->last <= offset + max)
		rangep->last = curp->last - offset;
	else
		rangep->last = max;

	return 1;
}


/*
 * pmempool_info_pool_hdr -- print pool header information
 */
static int
pmempool_info_pool_hdr(struct pmem_info *pip, int v)
{
	int ret = 0;
	struct pool_hdr *hdr = malloc(sizeof (struct pool_hdr));
	if (!hdr)
		err(1, "Cannot allocate memory for pool_hdr");

	if (pmempool_info_read(pip, hdr, sizeof (*hdr), 0)) {
		out_err("cannot read pool header\n");
		free(hdr);
		return -1;
	}

	outv(v, "POOL Header:\n");
	outv_hexdump(pip->args.vhdrdump, hdr, sizeof (*hdr), 0, 1);

	util_convert2h_pool_hdr(hdr);

	outv_field(v, "Signature", "%.*s", POOL_HDR_SIG_LEN,
			hdr->signature);
	outv_field(v, "Major", "%d", hdr->major);
	outv_field(v, "Mandatory features", "0x%x", hdr->incompat_features);
	outv_field(v, "Not mandatory features", "0x%x", hdr->compat_features);
	outv_field(v, "Forced RO", "0x%x", hdr->ro_compat_features);
	outv_field(v, "UUID", "%s", out_get_uuid_str(hdr->uuid));
	outv_field(v, "Parent UUID", "%s", out_get_uuid_str(hdr->parent_uuid));
	outv_field(v, "Previous part UUID", "%s",
				out_get_uuid_str(hdr->prev_part_uuid));
	outv_field(v, "Next part UUID", "%s",
				out_get_uuid_str(hdr->next_part_uuid));
	outv_field(v, "Previous replica UUID", "%s",
				out_get_uuid_str(hdr->prev_repl_uuid));
	outv_field(v, "Next replica UUID", "%s",
				out_get_uuid_str(hdr->next_repl_uuid));
	outv_field(v, "Creation Time", "%s",
			out_get_time_str((time_t)hdr->crtime));
	outv_field(v, "Checksum", "%s", out_get_checksum(hdr, sizeof (*hdr),
				&hdr->checksum));

	free(hdr);

	return ret;
}

/*
 * pmempool_info_log_data -- print used data from log pool
 */
static int
pmempool_info_log_data(struct pmem_info *pip, int v, struct pmemlog *plp)
{
	if (!outv_check(v))
		return 0;

	uint64_t size_used = plp->write_offset - plp->start_offset;

	if (size_used == 0)
		return 0;

	uint8_t *addr;
	if ((addr = mmap(NULL, size_used, PROT_READ|PROT_WRITE,
		MAP_PRIVATE|MAP_NORESERVE, pip->fd, plp->start_offset))
			== MAP_FAILED) {
		warn("%s", pip->file_name);
		out_err("cannot read pmem log data\n");
		return -1;
	}

	if (pip->args.walk == 0) {
		outv(v, "\nPMEMLOG data:\n");
		struct range *curp = NULL;
		LIST_FOREACH(curp, &pip->args.ranges.head, next) {
			uint8_t *ptr = addr + curp->first;
			if (curp->last >= size_used)
				curp->last = size_used - 1;
			uint64_t count = curp->last - curp->first + 1;
			outv_hexdump(v, ptr, count, curp->first +
					plp->start_offset, 1);
			size_used -= count;
			if (!size_used)
				break;
		}
	} else {

		/*
		 * Walk through used data with fixed chunk size
		 * passed by user.
		 */
		uint32_t nchunks = size_used / pip->args.walk;

		outv(v, "\nPMEMLOG data: [chunks: total = %lu size = %ld]\n",
				nchunks, pip->args.walk);

		struct range *curp = NULL;
		LIST_FOREACH(curp, &pip->args.ranges.head, next) {
			uint64_t i;
			for (i = curp->first; i <= curp->last &&
					i < nchunks; i++) {
				outv(v, "Chunk %10u:\n", i);
				outv_hexdump(v, addr + i * pip->args.walk,
					pip->args.walk,
					plp->start_offset + i * pip->args.walk,
					1);
			}
		}
	}

	return 0;
}

/*
 * pmempool_info_logs_stats -- print log type pool statistics
 */
static void
pmempool_info_log_stats(struct pmem_info *pip, int v, struct pmemlog *plp)
{
	uint64_t size_total = plp->end_offset - plp->start_offset;
	uint64_t size_used = plp->write_offset - plp->start_offset;
	uint64_t size_avail = size_total - size_used;

	if (size_total == 0)
		return;

	double perc_used = (double)size_used / (double)size_total * 100.0;
	double perc_avail =  100.0 - perc_used;

	outv(v, "\nPMEM LOG statistics:\n");
	outv_field(v, "Total", "%s",
			out_get_size_str(size_total, pip->args.human));
	outv_field(v, "Available", "%s [%s]",
			out_get_size_str(size_avail, pip->args.human),
			out_get_percentage(perc_avail));
	outv_field(v, "Used", "%s [%s]",
			out_get_size_str(size_used, pip->args.human),
			out_get_percentage(perc_used));

}

/*
 * pmempool_info_log -- print information about log type pool
 */
static int
pmempool_info_log(struct pmem_info *pip, int v)
{
	int ret = 0;

	struct pmemlog *plp = malloc(sizeof (struct pmemlog));
	if (!plp)
		err(1, "Cannot allocate memory for pmemlog structure");

	if (pmempool_info_read(pip, plp, sizeof (struct pmemlog), 0)) {
		out_err("cannot read pmemlog header\n");
		return -1;
	}

	outv(v, "\nPMEM LOG header:\n");

	/* dump pmemlog header without pool_hdr */
	outv_hexdump(pip->args.vhdrdump, (uint8_t *)plp + sizeof (plp->hdr),
			sizeof (*plp) - sizeof (plp->hdr),
			sizeof (plp->hdr), 1);

	util_convert2h_pmemlog(plp);

	int write_offset_valid = plp->write_offset >= plp->start_offset &&
				plp->write_offset <= plp->end_offset;
	outv_field(v, "Start offset", "0x%lx", plp->start_offset);
	outv_field(v, "Write offset", "0x%lx [%s]", plp->write_offset,
			write_offset_valid ? "OK":"ERROR");
	outv_field(v, "End offset", "0x%lx", plp->end_offset);

	if (write_offset_valid) {
		pmempool_info_log_stats(pip, pip->args.vstats, plp);
		ret = pmempool_info_log_data(pip, pip->args.vdata, plp);
	}

	free(plp);

	return ret;
}

/*
 * pmempool_info_btt_info -- print btt_info structure fields
 */
static int
pmempool_info_btt_info(struct pmem_info *pip, int v, struct btt_info *infop)
{
	outv_field(v, "Signature", "%.*s", BTTINFO_SIG_LEN, infop->sig);

	outv_field(v, "UUID of container", "%s",
			out_get_uuid_str(infop->parent_uuid));

	outv_field(v, "Flags", "0x%x", infop->flags);
	outv_field(v, "Major", "%d", infop->major);
	outv_field(v, "Minor", "%d", infop->minor);
	outv_field(v, "External LBA size", "%s",
			out_get_size_str(infop->external_lbasize,
				pip->args.human));
	outv_field(v, "External LBA count", "%u", infop->external_nlba);
	outv_field(v, "Internal LBA size", "%s",
			out_get_size_str(infop->internal_lbasize,
				pip->args.human));
	outv_field(v, "Internal LBA count", "%u", infop->internal_nlba);
	outv_field(v, "Free blocks", "%u", infop->nfree);
	outv_field(v, "Info block size", "%s",
		out_get_size_str(infop->infosize, pip->args.human));
	outv_field(v, "Next arena offset", "0x%lx", infop->nextoff);
	outv_field(v, "Arena data offset", "0x%lx", infop->dataoff);
	outv_field(v, "Area map offset", "0x%lx", infop->mapoff);
	outv_field(v, "Area flog offset", "0x%lx", infop->flogoff);
	outv_field(v, "Info block backup offset", "0x%lx", infop->infooff);
	outv_field(v, "Checksum", "%s", out_get_checksum(infop,
				sizeof (*infop), &infop->checksum));

	return 0;
}

/*
 * pmempool_info_blk_skip_block -- get action type for block/data chunk
 *
 * Return value indicating whether processing block/data chunk
 * should be skipped.
 *
 * Return values:
 *  0 - continue processing
 *  1 - skip current block
 */
static int
pmempool_info_blk_skip_block(struct pmem_info *pip, int is_zero,
		int is_error)
{
	if (pip->args.skip_no_flag && !is_zero && !is_error)
		return 1;

	if (is_zero && pip->args.skip_zeros)
		return 1;

	if (is_error && pip->args.skip_error)
		return 1;

	return 0;
}

/*
 * pmempool_info_btt_data -- print block data and corresponding flags from map
 */
static int
pmempool_info_btt_data(struct pmem_info *pip, int v,
	struct btt_info *infop, off_t arena_off, off_t offset, off_t *countp)
{
	if (!outv_check(v))
		return 0;

	int ret = 0;

	size_t mapsize = infop->external_nlba * BTT_MAP_ENTRY_SIZE;
	uint32_t *map = malloc(mapsize);
	if (!map)
		err(1, "Cannot allocate memory for BTT map");

	uint8_t *block_buff = malloc(infop->external_lbasize);
	if (!block_buff)
		err(1, "Cannot allocate memory for pmemblk block buffer");

	/* read btt map area */
	if (pmempool_info_read(pip, (uint8_t *)map, mapsize,
				arena_off + infop->mapoff)) {
		out_err("wrong BTT Map size or offset\n");
		ret = -1;
		goto error;
	}

	uint32_t i;
	struct range *curp = NULL;
	struct range range;
	LIST_FOREACH(curp, &pip->args.ranges.head, next) {
		if (pmempool_info_get_range(pip, &range, curp,
					infop->external_nlba - 1, offset) == 0)
			continue;
		for (i = range.first; i <= range.last; i++) {
			uint32_t map_entry = le32toh(map[i]);
			int is_init = (map_entry & ~BTT_MAP_ENTRY_LBA_MASK)
				== 0;
			int is_zero = (map_entry & ~BTT_MAP_ENTRY_LBA_MASK)
				== BTT_MAP_ENTRY_ZERO || is_init;
			int is_error = (map_entry & ~BTT_MAP_ENTRY_LBA_MASK)
				== BTT_MAP_ENTRY_ERROR;

			uint32_t blockno = is_init ? i :
					map_entry & BTT_MAP_ENTRY_LBA_MASK;

			if (pmempool_info_blk_skip_block(pip,
						is_zero, is_error))
				continue;

			/* compute block's data address */
			off_t block_off = arena_off + infop->dataoff +
				blockno * infop->internal_lbasize;

			if (pmempool_info_read(pip, block_buff,
					infop->external_lbasize, block_off)) {
				out_err("cannot read %d block\n", i);
				ret = -1;
				goto error;
			}

			if (*countp == 0)
				outv(v, "\nPMEM BLK blocks data:\n");

			/*
			 * Print block number, offset and flags
			 * from map entry.
			 */
			outv(v, "Block %10d: offset: %s\n",
				offset + i,
				out_get_btt_map_entry(map_entry));

			/* dump block's data */
			outv_hexdump(v, block_buff, infop->external_lbasize,
					block_off, 1);

			*countp = *countp + 1;
		}
	}
error:
	free(map);
	free(block_buff);
	return ret;
}

/*
 * pmempool_info_btt_map -- print all map entries
 */
static int
pmempool_info_btt_map(struct pmem_info *pip, int v,
		struct btt_info *infop, off_t arena_off,
		struct pmem_blk_stats *statsp, off_t offset, off_t *count)
{
	if (!outv_check(v) && !outv_check(pip->args.vstats))
		return 0;

	int ret = 0;
	size_t mapsize = infop->external_nlba * BTT_MAP_ENTRY_SIZE;

	uint32_t *map = malloc(mapsize);
	if (!map)
		err(1, "Cannot allocate memory for BTT map");

	/* read btt map area */
	if (pmempool_info_read(pip, (uint8_t *)map, mapsize,
				arena_off + infop->mapoff)) {
		out_err("wrong BTT Map size or offset\n");
		ret = -1;
		goto error;
	}

	uint32_t arena_count = 0;

	int i;
	struct range *curp = NULL;
	struct range range;
	LIST_FOREACH(curp, &pip->args.ranges.head, next) {
		if (pmempool_info_get_range(pip, &range, curp,
					infop->external_nlba - 1, offset) == 0)
			continue;
		for (i = range.first; i <= range.last; i++) {
			uint32_t entry = le32toh(map[i]);
			int is_zero = (entry & ~BTT_MAP_ENTRY_LBA_MASK) ==
				BTT_MAP_ENTRY_ZERO ||
				(entry & ~BTT_MAP_ENTRY_LBA_MASK) == 0;
			int is_error = (entry & ~BTT_MAP_ENTRY_LBA_MASK) ==
				BTT_MAP_ENTRY_ERROR;

			if (pmempool_info_blk_skip_block(pip,
					is_zero, is_error) == 0) {
				if (arena_count == 0)
					outv(v, "\nPMEM BLK BTT Map:\n");

				if (is_zero)
					statsp->zeros++;
				if (is_error)
					statsp->errors++;
				if (!is_zero && !is_error)
					statsp->noflag++;

				statsp->total++;

				arena_count++;
				(*count)++;

				outv(v, "%010d: %s\n", offset + i,
					out_get_btt_map_entry(entry));
			}
		}
	}
error:
	free(map);
	return ret;
}

/*
 * pmempool_info_btt_flog_convert -- convert flog entry
 */
static void
pmempool_info_btt_flog_convert(struct btt_flog *flogp)
{
	flogp->lba = le32toh(flogp->lba);
	flogp->old_map = le32toh(flogp->old_map);
	flogp->new_map = le32toh(flogp->new_map);
	flogp->seq = le32toh(flogp->seq);
}

/*
 * pmempool_info_btt_flog -- print all flog entries
 */
static int
pmempool_info_btt_flog(struct pmem_info *pip, int v,
		struct btt_info *infop, off_t arena_off)
{
	if (!outv_check(v))
		return 0;

	int ret = 0;
	struct btt_flog *flogp = NULL;
	struct btt_flog *flogpp = NULL;
	uint64_t flog_size = infop->nfree *
		roundup(2 * sizeof (struct btt_flog), BTT_FLOG_PAIR_ALIGN);
	flog_size = roundup(flog_size, BTT_ALIGNMENT);
	uint8_t *buff = malloc(flog_size);
	if (!buff)
		err(1, "Cannot allocate memory for FLOG entries");

	if (pmempool_info_read(pip, buff, flog_size,
				arena_off + infop->flogoff)) {
		out_err("cannot read BTT FLOG");
		ret = -1;
		goto error;
	}

	outv(v, "\nPMEM BLK BTT FLOG:\n");

	uint8_t *ptr = buff;
	int i;
	for (i = 0; i < infop->nfree; i++) {
		flogp = (struct btt_flog *)ptr;
		flogpp = flogp + 1;

		pmempool_info_btt_flog_convert(flogp);
		pmempool_info_btt_flog_convert(flogpp);

		outv(v, "%010d:\n", i);
		outv_field(v, "LBA", "0x%08x", flogp->lba);
		outv_field(v, "Old map", "0x%08x: %s", flogp->old_map,
			out_get_btt_map_entry(flogp->old_map));
		outv_field(v, "New map", "0x%08x: %s", flogp->new_map,
			out_get_btt_map_entry(flogp->new_map));
		outv_field(v, "Seq", "0x%x", flogp->seq);

		outv_field(v, "LBA'", "0x%08x", flogpp->lba);
		outv_field(v, "Old map'", "0x%08x: %s", flogpp->old_map,
			out_get_btt_map_entry(flogpp->old_map));
		outv_field(v, "New map'", "0x%08x: %s", flogpp->new_map,
			out_get_btt_map_entry(flogpp->new_map));
		outv_field(v, "Seq'", "0x%x", flogpp->seq);

		ptr += BTT_FLOG_PAIR_ALIGN;
	}
error:
	free(buff);
	return ret;
}

/*
 * pmempool_info_btt_stats -- print btt related statistics
 */
static void
pmempool_info_btt_stats(struct pmem_info *pip, int v,
		struct pmem_blk_stats *statsp)
{
	if (statsp->total > 0) {
		outv(v, "\nPMEM BLK Statistics:\n");
		double perc_zeros = (double)statsp->zeros /
			(double)statsp->total * 100.0;
		double perc_errors = (double)statsp->errors /
			(double)statsp->total * 100.0;
		double perc_noflag = (double)statsp->noflag /
			(double)statsp->total * 100.0;

		outv_field(v, "Total blocks", "%u", statsp->total);
		outv_field(v, "Zeroed blocks", "%u [%s]", statsp->zeros,
				out_get_percentage(perc_zeros));
		outv_field(v, "Error blocks", "%u [%s]", statsp->errors,
				out_get_percentage(perc_errors));
		outv_field(v, "Blocks without flag", "%u [%s]", statsp->noflag,
				out_get_percentage(perc_noflag));
	}
}

/*
 * pmempool_info_btt_layout -- print information about BTT layout
 */
static int
pmempool_info_btt_layout(struct pmem_info *pip, struct pmemblk *pbp,
		off_t btt_off)
{
	int ret = 0;

	if (btt_off <= 0) {
		out_err("wrong BTT layout offset\n");
		return -1;
	}

	struct btt_info *infop = NULL;

	struct pmem_blk_stats stats;
	pmem_blk_stats_reset(&stats);

	infop = malloc(sizeof (struct btt_info));
	if (!infop)
		err(1, "Cannot allocate memory for BTT Info structure");

	int narena = 0;
	off_t cur_lba = 0;
	off_t count_data = 0;
	off_t count_map = 0;
	off_t offset = btt_off;
	off_t nextoff = 0;

	do {
		/* read btt info area */
		if (pmempool_info_read(pip, infop, sizeof (*infop), offset)) {
			ret = -1;
			out_err("cannot read BTT Info header\n");
			goto err;
		}

		if (util_check_memory((uint8_t *)infop,
					sizeof (*infop), 0) == 0) {
			outv(1, "\n<No BTT layout>\n");
			break;
		}

		outv(1, "\n[ARENA %d]\nPMEM BLK BTT Info Header:\n", narena);
		outv_hexdump(pip->args.vhdrdump, infop,
				sizeof (*infop), offset, 1);

		util_convert2h_btt_info(infop);

		nextoff = infop->nextoff;

		/* print btt info fields */
		if (pmempool_info_btt_info(pip, 1, infop)) {
			ret = -1;
			goto err;
		}

		/* dump blocks data */
		if (pmempool_info_btt_data(pip, pip->args.vdata,
					infop, offset, cur_lba, &count_data)) {
			ret = -1;
			goto err;
		}

		/* print btt map entries and get statistics */
		if (pmempool_info_btt_map(pip, pip->args.vmap, infop,
					offset, &stats, cur_lba, &count_map)) {
			ret = -1;
			goto err;
		}

		/* print flog entries */
		if (pmempool_info_btt_flog(pip, pip->args.vflog, infop,
					offset)) {
			ret = -1;
			goto err;
		}

		/* increment LBA's counter before reading info backup */
		cur_lba += infop->external_nlba;

		/* read btt info backup area */
		if (pmempool_info_read(pip, infop, sizeof (*infop),
			offset + infop->infooff)) {
			out_err("wrong BTT Info Backup size or offset\n");
			ret = -1;
			goto err;
		}

		outv(pip->args.vbackup, "\nPMEM BLK BTT Info Header Backup:\n");
		if (outv_check(pip->args.vbackup))
			outv_hexdump(pip->args.vhdrdump, infop,
				sizeof (*infop),
				offset + infop->infooff, 1);

		util_convert2h_btt_info(infop);
		pmempool_info_btt_info(pip, pip->args.vbackup, infop);

		offset += nextoff;
		narena++;

	} while (nextoff > 0);

	pmempool_info_btt_stats(pip, pip->args.vstats, &stats);

err:
	if (infop)
		free(infop);

	return ret;
}

/*
 * pmempool_info_blk -- print information about block type pool
 */
static int
pmempool_info_blk(struct pmem_info *pip, int v)
{
	int ret;
	struct pmemblk *pbp = malloc(sizeof (struct pmemblk));
	if (!pbp)
		err(1, "Cannot allocate memory for pmemblk structure");

	if (pmempool_info_read(pip, pbp, sizeof (struct pmemblk), 0)) {
		out_err("cannot read pmemblk header\n");
		return -1;
	}

	outv(v, "\nPMEM BLK Header:\n");
	/* dump pmemblk header without pool_hdr */
	outv_hexdump(pip->args.vhdrdump, (uint8_t *)pbp + sizeof (pbp->hdr),
		sizeof (*pbp) - sizeof (pbp->hdr), sizeof (pbp->hdr), 1);
	outv_field(v, "Block size", "%s",
			out_get_size_str(pbp->bsize, pip->args.human));
	outv_field(v, "Is zeroed", pbp->is_zeroed ? "true" : "false");

	ssize_t btt_off = pbp->data - pbp->addr;
	ret = pmempool_info_btt_layout(pip, pbp, btt_off);

	free(pbp);

	return ret;
}

/*
 * pmempool_info_get_pool_type -- get pool type to parse
 *
 * Return pool type to parse based on headers data and command line arguments.
 */
static pmem_pool_type_t
pmempool_info_get_pool_type(struct pmem_info *pip)
{
	int ret = 0;

	struct pool_hdr *hdrp = malloc(sizeof (struct pool_hdr));
	if (!hdrp)
		err(1, "Cannot allocate memory for pool_hdr");

	if (pmempool_info_read(pip, hdrp, sizeof (*hdrp), 0)) {
		out_err("cannot read pool header\n");
		ret = PMEM_POOL_TYPE_UNKNOWN;
		goto error;
	}

	/*
	 * If force flag is set 'types' fields _must_ hold
	 * single pool type - this is validated when processing
	 * command line arguments.
	 */
	if (pip->args.force)
		return pip->args.type;

	/* parse pool type from pool header */
	ret = pmem_pool_type_parse_hdr(hdrp);
error:
	free(hdrp);
	return ret;
}

/*
 * pmempool_info_file -- print info about single file
 */
static int
pmempool_info_file(struct pmem_info *pip, const char *file_name)
{
	int ret = 0;

	pip->fd = open(file_name, O_RDONLY);
	if (pip->fd < 0) {
		warn("%s", file_name);
		return -1;
	}

	pip->file_name = file_name;

	/*
	 * Get pool type to parse based on headers
	 * and command line flags.
	 */
	pmem_pool_type_t type = pmempool_info_get_pool_type(pip);

	if (PMEM_POOL_TYPE_UNKNOWN == type) {
		/*
		 * This means don't know what pool type should be parsed
		 * this happens when can't determine pool type of file
		 * by parsing signature and force flag is not set.
		 */
		ret = -1;
		out_err("%s: cannot determine type of pool\n", file_name);
	} else {
		/*
		 * Print file name only when multiple files passed
		 * with single file - user knows what is processed.
		 */
		if (pip->args.n_files > 1)
			outv_field(1, "File", "%s", file_name);
		if (pmempool_info_pool_hdr(pip, 1)) {
			ret = -1;
			goto err;
		}

		switch (type) {
		case PMEM_POOL_TYPE_LOG:
			ret = pmempool_info_log(pip, 1);
			break;
		case PMEM_POOL_TYPE_BLK:
			ret = pmempool_info_blk(pip, 1);
			break;
		case PMEM_POOL_TYPE_UNKNOWN:
		default:
			ret = -1;
			break;
		}
	}
err:
	close(pip->fd);
	pip->fd = -1;

	return ret;
}

/*
 * pmempool_info_file_all -- print info about all input files
 */
static int
pmempool_info_file_all(struct pmem_info *pip)
{
	int i;
	int ret = 0;

	if (!pip->args.files)
		return -1;

	for (i = 0; i < pip->args.n_files; i++) {
		int fret = pmempool_info_file(pip, pip->args.files[i]);
		if (pip->args.n_files > 1 && i != pip->args.n_files - 1)
			outv(1, "\n");
		if (fret)
			ret = -1;
	}

	return ret;
}

/*
 * pmempool_info_alloc -- allocate pmem info context
 */
static struct pmem_info *
pmempool_info_alloc(void)
{
	struct pmem_info *pip = malloc(sizeof (struct pmem_info));
	if (!pip)
		err(1, "Cannot allocate memory for pmempool info context");

	if (pip) {
		memset(pip, 0, sizeof (*pip));
		/* set default command line parameters */
		memcpy(&pip->args, &pmempool_info_args_default,
				sizeof (pip->args));
		LIST_INIT(&pip->args.ranges.head);
	}

	return pip;
}

/*
 * pmempool_info_free -- free pmem info context
 */
static void
pmempool_info_free(struct pmem_info *pip)
{
	util_ranges_clear(&pip->args.ranges);
	free(pip);
}

int
pmempool_info_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	struct pmem_info *pip = pmempool_info_alloc();

	/* read command line arguments */
	if ((ret = parse_args(appname, argc, argv, &pip->args)) == 0) {
		/* set some output format values */
		out_set_vlevel(pip->args.vlevel);
		out_set_col_width(pip->args.col_width);

		/* process all files */
		ret = pmempool_info_file_all(pip);
	}

	pmempool_info_free(pip);

	return ret;
}
