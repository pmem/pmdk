// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * info.c -- pmempool info command main source file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <err.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/mman.h>

#include "common.h"
#include "output.h"
#include "out.h"
#include "info.h"
#include "set.h"
#include "file.h"
#include "badblocks.h"
#include "set_badblocks.h"

#define DEFAULT_CHUNK_TYPES\
	((1<<CHUNK_TYPE_FREE)|\
	(1<<CHUNK_TYPE_USED)|\
	(1<<CHUNK_TYPE_RUN))

#define GET_ALIGNMENT(ad, x)\
(1 + (((ad) >> (ALIGNMENT_DESC_BITS * (x))) & ((1 << ALIGNMENT_DESC_BITS) - 1)))

#define UNDEF_REPLICA UINT_MAX
#define UNDEF_PART UINT_MAX

/*
 * Default arguments
 */
static const struct pmempool_info_args pmempool_info_args_default = {
	/*
	 * Picked experimentally based on used fields names.
	 * This should be at least the number of characters of
	 * the longest field name.
	 */
	.col_width	= 24,
	.human		= false,
	.force		= false,
	.badblocks	= PRINT_BAD_BLOCKS_NOT_SET,
	.type		= PMEM_POOL_TYPE_UNKNOWN,
	.vlevel		= VERBOSE_DEFAULT,
	.vdata		= VERBOSE_SILENT,
	.vhdrdump	= VERBOSE_SILENT,
	.vstats		= VERBOSE_SILENT,
	.log		= {
		.walk		= 0,
	},
	.blk		= { /* deprecated */
		.vmap		= VERBOSE_SILENT,
		.vflog		= VERBOSE_SILENT,
		.vbackup	= VERBOSE_SILENT,
		.skip_zeros	= false,
		.skip_error	= false,
		.skip_no_flag	= false,
	},
	.obj		= {
		.vlanes		= VERBOSE_SILENT,
		.vroot		= VERBOSE_SILENT,
		.vobjects	= VERBOSE_SILENT,
		.valloc		= VERBOSE_SILENT,
		.voobhdr	= VERBOSE_SILENT,
		.vheap		= VERBOSE_SILENT,
		.vzonehdr	= VERBOSE_SILENT,
		.vchunkhdr	= VERBOSE_SILENT,
		.vbitmap	= VERBOSE_SILENT,
		.lanes_recovery	= false,
		.ignore_empty_obj = false,
		.chunk_types	= DEFAULT_CHUNK_TYPES,
		.replica	= 0,
	},
};

/*
 * long-options -- structure holding long options.
 */
static const struct option long_options[] = {
	{"version",	no_argument,		NULL, 'V' | OPT_ALL},
	{"verbose",	no_argument,		NULL, 'v' | OPT_ALL},
	{"help",	no_argument,		NULL, 'h' | OPT_ALL},
	{"human",	no_argument,		NULL, 'n' | OPT_ALL},
	{"force",	required_argument,	NULL, 'f' | OPT_ALL},
	{"data",	no_argument,		NULL, 'd' | OPT_ALL},
	{"headers-hex",	no_argument,		NULL, 'x' | OPT_ALL},
	{"stats",	no_argument,		NULL, 's' | OPT_ALL},
	{"range",	required_argument,	NULL, 'r' | OPT_ALL},
	{"bad-blocks",	required_argument,	NULL, 'k' | OPT_ALL},
	{"walk",	required_argument,	NULL, 'w' | OPT_LOG},
	{"skip-zeros",	no_argument,		NULL, 'z' | OPT_BLK | OPT_BTT},
	{"skip-error",	no_argument,		NULL, 'e' | OPT_BLK | OPT_BTT},
	{"skip-no-flag", no_argument,		NULL, 'u' | OPT_BLK | OPT_BTT},
	{"map",		no_argument,		NULL, 'm' | OPT_BLK | OPT_BTT},
	{"flog",	no_argument,		NULL, 'g' | OPT_BLK | OPT_BTT},
	{"backup",	no_argument,		NULL, 'B' | OPT_BLK | OPT_BTT},
	{"lanes",	no_argument,		NULL, 'l' | OPT_OBJ},
	{"recovery",	no_argument,		NULL, 'R' | OPT_OBJ},
	{"section",	required_argument,	NULL, 'S' | OPT_OBJ},
	{"object-store", no_argument,		NULL, 'O' | OPT_OBJ},
	{"types",	required_argument,	NULL, 't' | OPT_OBJ},
	{"no-empty",	no_argument,		NULL, 'E' | OPT_OBJ},
	{"alloc-header", no_argument,		NULL, 'A' | OPT_OBJ},
	{"oob-header",	no_argument,		NULL, 'a' | OPT_OBJ},
	{"root",	no_argument,		NULL, 'o' | OPT_OBJ},
	{"heap",	no_argument,		NULL, 'H' | OPT_OBJ},
	{"zones",	no_argument,		NULL, 'Z' | OPT_OBJ},
	{"chunks",	no_argument,		NULL, 'C' | OPT_OBJ},
	{"chunk-type",	required_argument,	NULL, 'T' | OPT_OBJ},
	{"bitmap",	no_argument,		NULL, 'b' | OPT_OBJ},
	{"replica",	required_argument,	NULL, 'p' | OPT_OBJ},
	{NULL,		0,			NULL,  0 },
};

static const struct option_requirement option_requirements[] = {
	{
		.opt	= 'r',
		.type	= PMEM_POOL_TYPE_LOG,
		.req	= OPT_REQ0('d')
	},
	{
		.opt	= 'r',
		.type	= PMEM_POOL_TYPE_BLK | PMEM_POOL_TYPE_BTT,
		.req	= OPT_REQ0('d') | OPT_REQ1('m')
	},
	{
		.opt	= 'z',
		.type	= PMEM_POOL_TYPE_BLK | PMEM_POOL_TYPE_BTT,
		.req	= OPT_REQ0('d') | OPT_REQ1('m')
	},
	{
		.opt	= 'e',
		.type	= PMEM_POOL_TYPE_BLK | PMEM_POOL_TYPE_BTT,
		.req	= OPT_REQ0('d') | OPT_REQ1('m')
	},
	{
		.opt	= 'u',
		.type	= PMEM_POOL_TYPE_BLK | PMEM_POOL_TYPE_BTT,
		.req	= OPT_REQ0('d') | OPT_REQ1('m')
	},
	{
		.opt	= 'r',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('Z') |
			OPT_REQ2('C') | OPT_REQ3('l'),
	},
	{
		.opt	= 'R',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('l')
	},
	{
		.opt	= 'S',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('l')
	},
	{
		.opt	= 'E',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O')
	},
	{
		.opt	= 'T',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('C')
	},
	{
		.opt	= 'b',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('H')
	},
	{
		.opt	= 'b',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('C')
	},
	{
		.opt	= 'A',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('l') | OPT_REQ2('o')
	},
	{
		.opt	= 'a',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('l') | OPT_REQ2('o')
	},
	{
		.opt	= 't',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('s'),
	},
	{
		.opt	= 'C',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('H') | OPT_REQ2('s'),
	},
	{
		.opt	= 'Z',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('H') | OPT_REQ2('s'),
	},
	{
		.opt	= 'd',
		.type	= PMEM_POOL_TYPE_OBJ,
		.req	= OPT_REQ0('O') | OPT_REQ1('o'),
	},
	{ 0,  0, 0}
};

/*
 * help_str -- string for help message
 */
static const char * const help_str =
"Show information about pmem pool from specified file.\n"
"NOTE: pmem blk pool is deprecated\n"
"\n"
"Common options:\n"
"  -h, --help                      Print this help and exit.\n"
"  -V, --version                   Print version and exit.\n"
"  -v, --verbose                   Increase verbisity level.\n"
"  -f, --force blk|log|obj|btt     Force parsing a pool of specified type.\n"
"  -n, --human                     Print sizes in human readable format.\n"
"  -x, --headers-hex               Hexdump all headers.\n"
"  -d, --data                      Dump log data and blocks.\n"
"  -s, --stats                     Print statistics.\n"
"  -r, --range <range>             Range of blocks/chunks/objects.\n"
"  -k, --bad-blocks=<yes|no>       Print bad blocks.\n"
"\n"
"Options for PMEMLOG:\n"
"  -w, --walk <size>               Chunk size.\n"
"\n"
"Options for PMEMBLK: (DEPRECATED)\n"
"  -m, --map                       Print BTT Map entries.\n"
"  -g, --flog                      Print BTT FLOG entries.\n"
"  -B, --backup                    Print BTT Info header backup.\n"
"  -z, --skip-zeros                Skip blocks marked with zero flag.\n"
"  -e, --skip-error                Skip blocks marked with error flag.\n"
"  -u, --skip-no-flag              Skip blocks not marked with any flag.\n"
"\n"
"Options for PMEMOBJ:\n"
"  -l, --lanes [<range>]           Print lanes from specified range.\n"
"  -R, --recovery                  Print only lanes which need recovery.\n"
"  -S, --section tx,allocator,list Print only specified sections.\n"
"  -O, --object-store              Print object store.\n"
"  -t, --types <range>             Specify objects' type numbers range.\n"
"  -E, --no-empty                  Print only non-empty object store lists.\n"
"  -o, --root                      Print root object information\n"
"  -A, --alloc-header              Print allocation header for objects in\n"
"                                  object store.\n"
"  -a, --oob-header                Print OOB header\n"
"  -H, --heap                      Print heap header.\n"
"  -Z, --zones [<range>]           Print zones header. If range is specified\n"
"                                  and --object|-O option is specified prints\n"
"                                  objects from specified zones only.\n"
"  -C, --chunks [<range>]          Print zones header. If range is specified\n"
"                                  and --object|-O option is specified prints\n"
"                                  objects from specified zones only.\n"
"  -T, --chunk-type used,free,run,footer\n"
"                                  Print only specified type(s) of chunk.\n"
"                                  [requires --chunks|-C]\n"
"  -b, --bitmap                    Print chunk run's bitmap in graphical\n"
"                                  format. [requires --chunks|-C]\n"
"  -p, --replica <num>             Print info from specified replica\n"
"For complete documentation see %s-info(1) manual page.\n"
;

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(const char *appname)
{
	printf("Usage: %s info [<args>] <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(const char *appname)
{
	printf("NOTE: pmem blk pool is deprecated\n");
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_info_help -- print application usage detailed description
 */
void
pmempool_info_help(const char *appname)
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
parse_args(const char *appname, int argc, char *argv[],
		struct pmempool_info_args *argsp,
		struct options *opts)
{
	int opt;

	if (argc == 1) {
		print_usage(appname);

		return -1;
	}

	struct ranges *rangesp = &argsp->ranges;
	while ((opt = util_options_getopt(argc, argv,
			"vhnf:ezuF:L:c:dmxVw:gBsr:lRS:OECZHT:bot:aAp:k:",
			opts)) != -1) {

		switch (opt) {
		case 'v':
			argsp->vlevel = VERBOSE_MAX;
			break;
		case 'V':
			print_version(appname);
			exit(EXIT_SUCCESS);
		case 'h':
			pmempool_info_help(appname);
			exit(EXIT_SUCCESS);
		case 'n':
			argsp->human = true;
			break;
		case 'f':
			argsp->type = pmem_pool_type_parse_str(optarg);
			if (argsp->type == PMEM_POOL_TYPE_UNKNOWN) {
				outv_err("'%s' -- unknown pool type\n", optarg);
				return -1;
			}
			argsp->force = true;
			break;
		case 'k':
			if (strcmp(optarg, "no") == 0) {
				argsp->badblocks = PRINT_BAD_BLOCKS_NO;
			} else if (strcmp(optarg, "yes") == 0) {
				argsp->badblocks = PRINT_BAD_BLOCKS_YES;
			} else {
				outv_err(
					"'%s' -- invalid argument of the '-k/--bad-blocks' option\n",
					optarg);
				return -1;
			}
			break;
		case 'e':
			argsp->blk.skip_error = true;
			break;
		case 'z':
			argsp->blk.skip_zeros = true;
			break;
		case 'u':
			argsp->blk.skip_no_flag = true;
			break;
		case 'r':
			if (util_parse_ranges(optarg, rangesp,
					ENTIRE_UINT64)) {
				outv_err("'%s' -- cannot parse range(s)\n",
						optarg);
				return -1;
			}

			if (rangesp == &argsp->ranges)
				argsp->use_range = 1;

			break;
		case 'd':
			argsp->vdata = VERBOSE_DEFAULT;
			break;
		case 'm':
			argsp->blk.vmap = VERBOSE_DEFAULT;
			break;
		case 'g':
			argsp->blk.vflog = VERBOSE_DEFAULT;
			break;
		case 'B':
			argsp->blk.vbackup = VERBOSE_DEFAULT;
			break;
		case 'x':
			argsp->vhdrdump = VERBOSE_DEFAULT;
			break;
		case 's':
			argsp->vstats = VERBOSE_DEFAULT;
			break;
		case 'w':
			argsp->log.walk = (size_t)atoll(optarg);
			if (argsp->log.walk == 0) {
				outv_err("'%s' -- invalid chunk size\n",
					optarg);
				return -1;
			}
			break;
		case 'l':
			argsp->obj.vlanes = VERBOSE_DEFAULT;
			rangesp = &argsp->obj.lane_ranges;
			break;
		case 'R':
			argsp->obj.lanes_recovery = true;
			break;
		case 'O':
			argsp->obj.vobjects = VERBOSE_DEFAULT;
			rangesp = &argsp->ranges;
			break;
		case 'a':
			argsp->obj.voobhdr = VERBOSE_DEFAULT;
			break;
		case 'A':
			argsp->obj.valloc = VERBOSE_DEFAULT;
			break;
		case 'E':
			argsp->obj.ignore_empty_obj = true;
			break;
		case 'Z':
			argsp->obj.vzonehdr = VERBOSE_DEFAULT;
			rangesp = &argsp->obj.zone_ranges;
			break;
		case 'C':
			argsp->obj.vchunkhdr = VERBOSE_DEFAULT;
			rangesp = &argsp->obj.chunk_ranges;
			break;
		case 'H':
			argsp->obj.vheap = VERBOSE_DEFAULT;
			break;
		case 'T':
			argsp->obj.chunk_types = 0;
			if (util_parse_chunk_types(optarg,
					&argsp->obj.chunk_types) ||
				(argsp->obj.chunk_types &
				(1 << CHUNK_TYPE_UNKNOWN))) {
				outv_err("'%s' -- cannot parse chunk type(s)\n",
						optarg);
				return -1;
			}
			break;
		case 'o':
			argsp->obj.vroot = VERBOSE_DEFAULT;
			break;
		case 't':
			if (util_parse_ranges(optarg,
				&argsp->obj.type_ranges, ENTIRE_UINT64)) {
				outv_err("'%s' -- cannot parse range(s)\n",
						optarg);
				return -1;
			}
			break;
		case 'b':
			argsp->obj.vbitmap = VERBOSE_DEFAULT;
			break;
		case 'p':
		{
			char *endptr;
			int olderrno = errno;
			errno = 0;
			long long ll = strtoll(optarg, &endptr, 10);
			if ((endptr && *endptr != '\0') || errno) {
				outv_err("'%s' -- invalid replica number",
						optarg);
				return -1;
			}
			errno = olderrno;
			argsp->obj.replica = (size_t)ll;
			break;
		}
		default:
			print_usage(appname);
			return -1;
		}
	}

	if (optind < argc) {
		argsp->file = argv[optind];
	} else {
		print_usage(appname);
		return -1;
	}

	if (!argsp->use_range)
		util_ranges_add(&argsp->ranges, ENTIRE_UINT64);

	if (util_ranges_empty(&argsp->obj.type_ranges))
		util_ranges_add(&argsp->obj.type_ranges, ENTIRE_UINT64);

	if (util_ranges_empty(&argsp->obj.lane_ranges))
		util_ranges_add(&argsp->obj.lane_ranges, ENTIRE_UINT64);

	if (util_ranges_empty(&argsp->obj.zone_ranges))
		util_ranges_add(&argsp->obj.zone_ranges, ENTIRE_UINT64);

	if (util_ranges_empty(&argsp->obj.chunk_ranges))
		util_ranges_add(&argsp->obj.chunk_ranges, ENTIRE_UINT64);

	return 0;
}

/*
 * pmempool_info_read -- read data from file
 */
int
pmempool_info_read(struct pmem_info *pip, void *buff, size_t nbytes,
		uint64_t off)
{
	return pool_set_file_read(pip->pfile, buff, nbytes, off);
}

/*
 * pmempool_info_badblocks -- (internal) prints info about file badblocks
 */
static int
pmempool_info_badblocks(struct pmem_info *pip, const char *file_name, int v)
{
	int ret;

	if (pip->args.badblocks != PRINT_BAD_BLOCKS_YES)
		return 0;

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	ret = badblocks_get(file_name, bbs);
	if (ret) {
		if (errno == ENOTSUP) {
			outv(v, BB_NOT_SUPP "\n");
			ret = -1;
			goto exit_free;
		}

		outv_err("checking bad blocks failed -- '%s'", file_name);
		goto exit_free;
	}

	if (bbs->bb_cnt == 0 || bbs->bbv == NULL)
		goto exit_free;

	outv(v, "bad blocks:\n");
	outv(v, "\toffset\t\tlength\n");

	unsigned b;
	for (b = 0; b < bbs->bb_cnt; b++) {
		outv(v, "\t%zu\t\t%zu\n",
			B2SEC(bbs->bbv[b].offset),
			B2SEC(bbs->bbv[b].length));
	}

exit_free:
	badblocks_delete(bbs);

	return ret;
}

/*
 * pmempool_info_part -- (internal) print info about poolset part
 */
static int
pmempool_info_part(struct pmem_info *pip, unsigned repn, unsigned partn, int v)
{
	/* get path of the part file */
	const char *path = NULL;
	if (repn != UNDEF_REPLICA && partn != UNDEF_PART) {
		outv(v, "part %u:\n", partn);
		struct pool_set_part *part =
			&pip->pfile->poolset->replica[repn]->part[partn];
		path = part->path;
	} else {
		outv(v, "Part file:\n");
		path = pip->file_name;
	}
	outv_field(v, "path", "%s", path);

	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	const char *type_str = type == TYPE_DEVDAX ? "device dax" :
				"regular file";
	outv_field(v, "type", "%s", type_str);

	/* get size of the part file */
	ssize_t size = util_file_get_size(path);
	if (size < 0) {
		outv_err("couldn't get size of %s", path);
		return -1;
	}
	outv_field(v, "size", "%s", out_get_size_str((size_t)size,
			pip->args.human));

	/* get alignment of device dax */
	if (type == TYPE_DEVDAX) {
		size_t alignment = util_file_device_dax_alignment(path);
		outv_field(v, "alignment", "%s", out_get_size_str(alignment,
				pip->args.human));
	}

	/* look for bad blocks */
	if (pmempool_info_badblocks(pip, path, VERBOSE_DEFAULT)) {
		outv_err("Unable to retrieve badblock info");
		return -1;
	}

	return 0;
}

/*
 * pmempool_info_directory -- (internal) print information about directory
 */
static void
pmempool_info_directory(struct pool_set_directory *d,
	int v)
{
	outv(v, "Directory %s:\n", d->path);
	outv_field(v, "reservation size", "%lu", d->resvsize);
}

/*
 * pmempool_info_replica -- (internal) print info about replica
 */
static int
pmempool_info_replica(struct pmem_info *pip, unsigned repn, int v)
{
	struct pool_replica *rep = pip->pfile->poolset->replica[repn];
	outv(v, "Replica %u%s - local", repn,
		repn == 0 ? " (master)" : "");

	outv(v, ", %u part(s):\n", rep->nparts);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		if (pmempool_info_part(pip, repn, p, v))
			return -1;
	}

	if (pip->pfile->poolset->directory_based) {
		size_t nd = VEC_SIZE(&rep->directory);
		outv(v, "%lu %s:\n", nd, nd == 1 ? "Directory" : "Directories");
		struct pool_set_directory *d;
		VEC_FOREACH_BY_PTR(d, &rep->directory) {
			pmempool_info_directory(d, v);
		}
	}

	return 0;
}

/*
 * pmempool_info_poolset -- (internal) print info about poolset structure
 */
static int
pmempool_info_poolset(struct pmem_info *pip, int v)
{
	ASSERTeq(pip->params.is_poolset, 1);
	if (pip->pfile->poolset->directory_based)
		outv(v, "Directory-based Poolset structure:\n");
	else
		outv(v, "Poolset structure:\n");

	outv_field(v, "Number of replicas", "%u",
			pip->pfile->poolset->nreplicas);
	for (unsigned r = 0; r < pip->pfile->poolset->nreplicas; ++r) {
		if (pmempool_info_replica(pip, r, v))
			return -1;
	}

	if (pip->pfile->poolset->options > 0) {
		outv_title(v, "Poolset options");
		if (pip->pfile->poolset->options & OPTION_SINGLEHDR)
			outv(v, "%s", "SINGLEHDR\n");
	}

	return 0;
}

/*
 * pmempool_info_pool_hdr -- (internal) print pool header information
 */
static int
pmempool_info_pool_hdr(struct pmem_info *pip, int v)
{
	static const char *alignment_desc_str[] = {
		"  char",
		"  short",
		"  int",
		"  long",
		"  long long",
		"  size_t",
		"  os_off_t",
		"  float",
		"  double",
		"  long double",
		"  void *",
	};
	static const size_t alignment_desc_n =
		sizeof(alignment_desc_str) / sizeof(alignment_desc_str[0]);

	int ret = 0;
	struct pool_hdr *hdr = malloc(sizeof(struct pool_hdr));
	if (!hdr)
		err(1, "Cannot allocate memory for pool_hdr");

	if (pmempool_info_read(pip, hdr, sizeof(*hdr), 0)) {
		outv_err("cannot read pool header\n");
		free(hdr);
		return -1;
	}

	struct arch_flags arch_flags;
	util_get_arch_flags(&arch_flags);

	outv_title(v, "POOL Header");
	outv_hexdump(pip->args.vhdrdump, hdr, sizeof(*hdr), 0, 1);

	util_convert2h_hdr_nocheck(hdr);

	outv_field(v, "Signature", "%.*s%s", POOL_HDR_SIG_LEN,
			hdr->signature,
			pip->params.is_part ?
			" [part file]" : "");
	outv_field(v, "Major", "%d", hdr->major);
	outv_field(v, "Mandatory features", "%s",
			out_get_incompat_features_str(hdr->features.incompat));
	outv_field(v, "Not mandatory features", "0x%x", hdr->features.compat);
	outv_field(v, "Forced RO", "0x%x", hdr->features.ro_compat);
	outv_field(v, "Pool set UUID", "%s",
				out_get_uuid_str(hdr->poolset_uuid));
	outv_field(v, "UUID", "%s", out_get_uuid_str(hdr->uuid));
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

	uint64_t ad = hdr->arch_flags.alignment_desc;
	uint64_t cur_ad = arch_flags.alignment_desc;

	outv_field(v, "Alignment Descriptor", "%s",
			out_get_alignment_desc_str(ad, cur_ad));

	for (size_t i = 0; i < alignment_desc_n; i++) {
		uint64_t a = GET_ALIGNMENT(ad, i);
		if (ad == cur_ad) {
			outv_field(v + 1, alignment_desc_str[i],
					"%2lu", a);
		} else {
			uint64_t av = GET_ALIGNMENT(cur_ad, i);
			if (a == av) {
				outv_field(v + 1, alignment_desc_str[i],
					"%2lu [OK]", a);
			} else {
				outv_field(v + 1, alignment_desc_str[i],
					"%2lu [wrong! should be %2lu]", a, av);
			}
		}
	}

	outv_field(v, "Class", "%s",
			out_get_arch_machine_class_str(
				hdr->arch_flags.machine_class));
	outv_field(v, "Data", "%s",
			out_get_arch_data_str(hdr->arch_flags.data));
	outv_field(v, "Machine", "%s",
			out_get_arch_machine_str(hdr->arch_flags.machine));
	outv_field(v, "Last shutdown", "%s",
			out_get_last_shutdown_str(hdr->sds.dirty));
	outv_field(v, "Checksum", "%s", out_get_checksum(hdr, sizeof(*hdr),
			&hdr->checksum, POOL_HDR_CSUM_END_OFF(hdr)));

	free(hdr);

	return ret;
}

/*
 * pmempool_info_file -- print info about single file
 */
static int
pmempool_info_file(struct pmem_info *pip, const char *file_name)
{
	int ret = 0;

	pip->file_name = file_name;

	/*
	 * If force flag is set 'types' fields _must_ hold
	 * single pool type - this is validated when processing
	 * command line arguments.
	 */
	if (pip->args.force) {
		pip->type = pip->args.type;
	} else {
		if (pmem_pool_parse_params(file_name, &pip->params, 1)) {
			if (errno)
				perror(file_name);
			else
				outv_err("%s: cannot determine type of pool\n",
					file_name);
			return -1;
		}

		pip->type = pip->params.type;
	}

	if (PMEM_POOL_TYPE_UNKNOWN == pip->type) {
		outv_err("%s: unknown pool type -- '%s'\n", file_name,
				pip->params.signature);
		return -1;
	} else if (!pip->args.force && !pip->params.is_checksum_ok) {
		outv_err("%s: invalid checksum\n", file_name);
		return -1;
	} else {
		if (util_options_verify(pip->opts, pip->type))
			return -1;

		pip->pfile = pool_set_file_open(file_name, 0, !pip->args.force);
		if (!pip->pfile) {
			perror(file_name);
			return -1;
		}

		/* check if we should check and print bad blocks */
		if (pip->args.badblocks == PRINT_BAD_BLOCKS_NOT_SET) {
			struct pool_hdr hdr;
			if (pmempool_info_read(pip, &hdr, sizeof(hdr), 0)) {
				outv_err("cannot read pool header\n");
				goto out_close;
			}
			util_convert2h_hdr_nocheck(&hdr);
			if (hdr.features.compat & POOL_FEAT_CHECK_BAD_BLOCKS)
				pip->args.badblocks = PRINT_BAD_BLOCKS_YES;
			else
				pip->args.badblocks = PRINT_BAD_BLOCKS_NO;
		}

		if (pip->type != PMEM_POOL_TYPE_BTT) {
			struct pool_set *ps = pip->pfile->poolset;
			for (unsigned r = 0; r < ps->nreplicas; ++r) {
				if (mprotect(ps->replica[r]->part[0].addr,
					ps->replica[r]->repsize,
					PROT_READ) < 0) {
					outv_err(
					"%s: failed to change pool protection",
					pip->pfile->fname);

					ret = -1;
					goto out_close;
				}
			}
		}

		if (pip->args.obj.replica) {
			size_t nreplicas = pool_set_file_nreplicas(pip->pfile);
			if (nreplicas == 1) {
				outv_err("only master replica available");
				ret = -1;
				goto out_close;
			}

			if (pip->args.obj.replica >= nreplicas) {
				outv_err("replica number out of range"
					" (valid range is: 0-%" PRIu64 ")",
					nreplicas - 1);
				ret = -1;
				goto out_close;
			}

			if (pool_set_file_set_replica(pip->pfile,
				pip->args.obj.replica)) {
				outv_err("setting replica number failed");
				ret = -1;
				goto out_close;
			}
		}

		/* hdr info is not present in btt device */
		if (pip->type != PMEM_POOL_TYPE_BTT) {
			if (pip->params.is_poolset &&
					pmempool_info_poolset(pip,
							VERBOSE_DEFAULT)) {
				ret = -1;
				goto out_close;
			}
			if (!pip->params.is_poolset &&
					pmempool_info_part(pip, UNDEF_REPLICA,
						UNDEF_PART, VERBOSE_DEFAULT)) {
				ret = -1;
				goto out_close;
			}
			if (pmempool_info_pool_hdr(pip, VERBOSE_DEFAULT)) {
				ret = -1;
				goto out_close;
			}
		}

		if (pip->params.is_part) {
			ret = 0;
			goto out_close;
		}

		switch (pip->type) {
		case PMEM_POOL_TYPE_LOG:
			ret = pmempool_info_log(pip);
			break;
		case PMEM_POOL_TYPE_BLK:
			ret = pmempool_info_blk(pip);
			break;
		case PMEM_POOL_TYPE_OBJ:
			ret = pmempool_info_obj(pip);
			break;
		case PMEM_POOL_TYPE_BTT:
			ret = pmempool_info_btt(pip);
			break;
		case PMEM_POOL_TYPE_UNKNOWN:
		default:
			ret = -1;
			break;
		}
out_close:
		pool_set_file_close(pip->pfile);
	}

	return ret;
}

/*
 * pmempool_info_alloc -- allocate pmem info context
 */
static struct pmem_info *
pmempool_info_alloc(void)
{
	struct pmem_info *pip = malloc(sizeof(struct pmem_info));
	if (!pip)
		err(1, "Cannot allocate memory for pmempool info context");

	if (pip) {
		memset(pip, 0, sizeof(*pip));

		/* set default command line parameters */
		memcpy(&pip->args, &pmempool_info_args_default,
				sizeof(pip->args));
		pip->opts = util_options_alloc(long_options,
				sizeof(long_options) /
				sizeof(long_options[0]),
				option_requirements);

		PMDK_LIST_INIT(&pip->args.ranges.head);
		PMDK_LIST_INIT(&pip->args.obj.type_ranges.head);
		PMDK_LIST_INIT(&pip->args.obj.lane_ranges.head);
		PMDK_LIST_INIT(&pip->args.obj.zone_ranges.head);
		PMDK_LIST_INIT(&pip->args.obj.chunk_ranges.head);
		PMDK_TAILQ_INIT(&pip->obj.stats.type_stats);
	}

	return pip;
}

/*
 * pmempool_info_free -- free pmem info context
 */
static void
pmempool_info_free(struct pmem_info *pip)
{
	if (pip->obj.stats.zone_stats) {
		for (uint64_t i = 0; i < pip->obj.stats.n_zones; ++i)
			VEC_DELETE(&pip->obj.stats.zone_stats[i].class_stats);

		free(pip->obj.stats.zone_stats);
	}
	util_options_free(pip->opts);
	util_ranges_clear(&pip->args.ranges);
	util_ranges_clear(&pip->args.obj.type_ranges);
	util_ranges_clear(&pip->args.obj.zone_ranges);
	util_ranges_clear(&pip->args.obj.chunk_ranges);
	util_ranges_clear(&pip->args.obj.lane_ranges);

	while (!PMDK_TAILQ_EMPTY(&pip->obj.stats.type_stats)) {
		struct pmem_obj_type_stats *type =
			PMDK_TAILQ_FIRST(&pip->obj.stats.type_stats);
		PMDK_TAILQ_REMOVE(&pip->obj.stats.type_stats, type, next);
		free(type);
	}

	free(pip);
}

int
pmempool_info_func(const char *appname, int argc, char *argv[])
{
	int ret = 0;
	struct pmem_info *pip = pmempool_info_alloc();

	/* read command line arguments */
	if ((ret = parse_args(appname, argc, argv, &pip->args,
					pip->opts)) == 0) {
		/* set some output format values */
		out_set_vlevel(pip->args.vlevel);
		out_set_col_width(pip->args.col_width);

		ret = pmempool_info_file(pip, pip->args.file);
	}

	pmempool_info_free(pip);

	return ret;
}
