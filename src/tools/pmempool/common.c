// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

/*
 * common.c -- definitions of common functions
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <ctype.h>
#include <assert.h>
#include <getopt.h>
#include <unistd.h>
#include <endian.h>

#include "common.h"

#include "output.h"
#include "libpmem.h"
#include "libpmemblk.h"
#include "libpmemlog.h"
#include "libpmemobj.h"
#include "btt.h"
#include "file.h"
#include "os.h"
#include "set.h"
#include "out.h"
#include "mmap.h"
#include "util_pmem.h"
#include "set_badblocks.h"
#include "util.h"

#define REQ_BUFF_SIZE	2048U
#define Q_BUFF_SIZE	8192
typedef const char *(*enum_to_str_fn)(int);

/*
 * pmem_pool_type -- return pool type based on first two pages.
 * If pool header's content suggests that pool may be BTT device
 * (first page zeroed and no correct signature for pool header),
 * signature from second page is checked to prove that it's BTT device layout.
 */
pmem_pool_type_t
pmem_pool_type(const void *base_pool_addr)
{
	struct pool_hdr *hdrp = (struct pool_hdr *)base_pool_addr;

	if (util_is_zeroed(hdrp, DEFAULT_HDR_SIZE)) {
		return util_get_pool_type_second_page(base_pool_addr);
	}

	pmem_pool_type_t type = pmem_pool_type_parse_hdr(hdrp);
	if (type != PMEM_POOL_TYPE_UNKNOWN)
		return type;
	else
		return util_get_pool_type_second_page(base_pool_addr);
}

/*
 * pmem_pool_checksum -- return true if checksum is correct
 * based on first two pages
 */
int
pmem_pool_checksum(const void *base_pool_addr)
{
	/* check whether it's btt device -> first page zeroed */
	if (util_is_zeroed(base_pool_addr, DEFAULT_HDR_SIZE)) {
		struct btt_info bttinfo;
		void *sec_page_addr = (char *)base_pool_addr + DEFAULT_HDR_SIZE;
		memcpy(&bttinfo, sec_page_addr, sizeof(bttinfo));
		btt_info_convert2h(&bttinfo);
		return util_checksum(&bttinfo, sizeof(bttinfo),
			&bttinfo.checksum, 0, 0);
	} else {
		/* it's not btt device - first page contains header */
		struct pool_hdr hdrp;
		memcpy(&hdrp, base_pool_addr, sizeof(hdrp));
		return util_checksum(&hdrp, sizeof(hdrp),
			&hdrp.checksum, 0, POOL_HDR_CSUM_END_OFF(&hdrp));
	}
}

/*
 * pmem_pool_type_parse_hdr -- return pool type based only on signature
 */
pmem_pool_type_t
pmem_pool_type_parse_hdr(const struct pool_hdr *hdrp)
{
	if (memcmp(hdrp->signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_LOG;
	else if (memcmp(hdrp->signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_BLK;
	else if (memcmp(hdrp->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_OBJ;
	else
		return PMEM_POOL_TYPE_UNKNOWN;
}

/*
 * pmem_pool_type_parse_str -- returns pool type from command line arg
 */
pmem_pool_type_t
pmem_pool_type_parse_str(const char *str)
{
	if (strcmp(str, "blk") == 0) {
		return PMEM_POOL_TYPE_BLK;
	} else if (strcmp(str, "log") == 0) {
		return PMEM_POOL_TYPE_LOG;
	} else if (strcmp(str, "obj") == 0) {
		return PMEM_POOL_TYPE_OBJ;
	} else if (strcmp(str, "btt") == 0) {
		return PMEM_POOL_TYPE_BTT;
	} else {
		return PMEM_POOL_TYPE_UNKNOWN;
	}
}

/*
 * util_get_pool_type_second_page -- return type based on second page content
 */
pmem_pool_type_t
util_get_pool_type_second_page(const void *pool_base_addr)
{
	struct btt_info bttinfo;

	void *sec_page_addr = (char *)pool_base_addr + DEFAULT_HDR_SIZE;
	memcpy(&bttinfo, sec_page_addr, sizeof(bttinfo));
	btt_info_convert2h(&bttinfo);

	if (util_is_zeroed(&bttinfo, sizeof(bttinfo)))
		return PMEM_POOL_TYPE_UNKNOWN;

	if (memcmp(bttinfo.sig, BTTINFO_SIG, BTTINFO_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_BTT;

	return PMEM_POOL_TYPE_UNKNOWN;
}

/*
 * util_parse_mode -- parse file mode from octal string
 */
int
util_parse_mode(const char *str, mode_t *mode)
{
	mode_t m = 0;
	int digits = 0;

	/* skip leading zeros */
	while (*str == '0')
		str++;

	/* parse at most 3 octal digits */
	while (digits < 3 && *str != '\0') {
		if (*str < '0' || *str > '7')
			return -1;
		m = (mode_t)(m << 3) | (mode_t)(*str - '0');
		digits++;
		str++;
	}

	/* more than 3 octal digits */
	if (digits == 3 && *str != '\0')
		return -1;

	if (mode)
		*mode = m;

	return 0;
}

static void
util_range_limit(struct range *rangep, struct range limit)
{
	if (rangep->first < limit.first)
		rangep->first = limit.first;
	if (rangep->last > limit.last)
		rangep->last = limit.last;
}

/*
 * util_parse_range_from -- parse range string as interval from specified number
 */
static int
util_parse_range_from(char *str, struct range *rangep, struct range entire)
{
	size_t str_len = strlen(str);
	if (str[str_len - 1] != '-')
		return -1;

	str[str_len - 1] = '\0';

	if (util_parse_size(str, (size_t *)&rangep->first))
		return -1;

	rangep->last = entire.last;
	util_range_limit(rangep, entire);

	return 0;
}

/*
 * util_parse_range_to -- parse range string as interval to specified number
 */
static int
util_parse_range_to(char *str, struct range *rangep, struct range entire)
{

	if (str[0] != '-' || str[1] == '\0')
		return -1;

	if (util_parse_size(str + 1, (size_t *)&rangep->last))
		return -1;

	rangep->first = entire.first;
	util_range_limit(rangep, entire);

	return 0;
}

/*
 * util_parse_range_number -- parse range string as a single number
 */
static int
util_parse_range_number(char *str, struct range *rangep, struct range entire)
{
	if (util_parse_size(str, (size_t *)&rangep->first) != 0)
		return -1;
	rangep->last = rangep->first;
	if (rangep->first > entire.last ||
	    rangep->last < entire.first)
		return -1;
	util_range_limit(rangep, entire);
	return 0;
}

/*
 * util_parse_range -- parse single range string
 */
static int
util_parse_range(char *str, struct range *rangep, struct range entire)
{
	char *dash = strchr(str, '-');
	if (!dash)
		return util_parse_range_number(str, rangep, entire);

	/* '-' at the beginning */
	if (dash == str)
		return util_parse_range_to(str, rangep, entire);

	/* '-' at the end */
	if (dash[1] == '\0')
		return util_parse_range_from(str, rangep, entire);

	*dash = '\0';
	dash++;

	if (util_parse_size(str, (size_t *)&rangep->first))
		return -1;

	if (util_parse_size(dash, (size_t *)&rangep->last))
		return -1;

	if (rangep->first > rangep->last) {
		uint64_t tmp = rangep->first;
		rangep->first = rangep->last;
		rangep->last = tmp;
	}

	util_range_limit(rangep, entire);

	return 0;
}

/*
 * util_ranges_overlap -- return 1 if two ranges are overlapped
 */
static int
util_ranges_overlap(struct range *rangep1, struct range *rangep2)
{
	if (rangep1->last + 1 < rangep2->first ||
	    rangep2->last + 1 < rangep1->first)
		return 0;
	else
		return 1;
}

/*
 * util_ranges_add -- create and add range
 */
int
util_ranges_add(struct ranges *rangesp, struct range range)
{
	struct range *rangep = malloc(sizeof(struct range));
	if (!rangep)
		err(1, "Cannot allocate memory for range\n");
	memcpy(rangep, &range, sizeof(*rangep));

	struct range *curp, *next;
	uint64_t first = rangep->first;
	uint64_t last = rangep->last;

	curp = PMDK_LIST_FIRST(&rangesp->head);
	while (curp) {
		next = PMDK_LIST_NEXT(curp, next);
		if (util_ranges_overlap(curp, rangep)) {
			PMDK_LIST_REMOVE(curp, next);
			if (curp->first < first)
				first = curp->first;
			if (curp->last > last)
				last = curp->last;
			free(curp);
		}
		curp = next;
	}

	rangep->first = first;
	rangep->last = last;

	PMDK_LIST_FOREACH(curp, &rangesp->head, next) {
		if (curp->first < rangep->first) {
			PMDK_LIST_INSERT_AFTER(curp, rangep, next);
			return 0;
		}
	}

	PMDK_LIST_INSERT_HEAD(&rangesp->head, rangep, next);

	return 0;
}

/*
 * util_ranges_contain -- return 1 if ranges contain the number n
 */
int
util_ranges_contain(const struct ranges *rangesp, uint64_t n)
{
	struct range *curp  = NULL;
	PMDK_LIST_FOREACH(curp, &rangesp->head, next) {
		if (curp->first <= n && n <= curp->last)
			return 1;
	}

	return 0;
}

/*
 * util_ranges_empty -- return 1 if ranges are empty
 */
int
util_ranges_empty(const struct ranges *rangesp)
{
	return PMDK_LIST_EMPTY(&rangesp->head);
}

/*
 * util_ranges_clear -- clear list of ranges
 */
void
util_ranges_clear(struct ranges *rangesp)
{
	while (!PMDK_LIST_EMPTY(&rangesp->head)) {
		struct range *rangep = PMDK_LIST_FIRST(&rangesp->head);
		PMDK_LIST_REMOVE(rangep, next);
		free(rangep);
	}
}

/*
 * util_parse_ranges -- parser ranges from string
 *
 * The valid formats of range are:
 * - 'n-m' -- from n to m
 * - '-m'  -- from minimum passed in entirep->first to m
 * - 'n-'  -- from n to maximum passed in entirep->last
 * - 'n'   -- n'th byte/block
 * Multiple ranges may be separated by comma:
 * 'n1-m1,n2-,-m3,n4'
 */
int
util_parse_ranges(const char *ptr, struct ranges *rangesp, struct range entire)
{
	if (ptr == NULL)
		return util_ranges_add(rangesp, entire);

	char *dup = strdup(ptr);
	if (!dup)
		err(1, "Cannot allocate memory for ranges");
	char *str = dup;
	int ret = 0;
	char *next = str;
	do {
		str = next;
		next = strchr(str, ',');
		if (next != NULL) {
			*next = '\0';
			next++;
		}
		struct range range;
		if (util_parse_range(str, &range, entire)) {
			ret = -1;
			goto out;
		} else if (util_ranges_add(rangesp, range)) {
			ret = -1;
			goto out;
		}
	} while (next != NULL);
out:
	free(dup);
	return ret;
}

/*
 * pmem_pool_get_min_size -- return minimum size of pool for specified type
 */
uint64_t
pmem_pool_get_min_size(pmem_pool_type_t type)
{
	switch (type) {
	case PMEM_POOL_TYPE_LOG:
		return PMEMLOG_MIN_POOL;
	case PMEM_POOL_TYPE_BLK:
		return PMEMBLK_MIN_POOL;
	case PMEM_POOL_TYPE_OBJ:
		return PMEMOBJ_MIN_POOL;
	default:
		break;
	}

	return 0;
}

/*
 * util_poolset_map -- map poolset
 */
int
util_poolset_map(const char *fname, struct pool_set **poolset, int rdonly)
{
	if (util_is_poolset_file(fname) != 1) {
		int ret = util_poolset_create_set(poolset, fname, 0, 0, true);
		if (ret < 0) {
			outv_err("cannot open pool set -- '%s'", fname);
			return -1;
		}
		unsigned flags = (rdonly ? POOL_OPEN_COW : 0) |
					POOL_OPEN_IGNORE_BAD_BLOCKS;
		return util_pool_open_nocheck(*poolset, flags);
	}

	/* open poolset file */
	int fd = util_file_open(fname, NULL, 0, O_RDONLY);
	if (fd < 0)
		return -1;

	struct pool_set *set;

	/* parse poolset file */
	if (util_poolset_parse(&set, fname, fd)) {
		outv_err("parsing poolset file failed\n");
		os_close(fd);
		return -1;
	}
	set->ignore_sds = true;
	os_close(fd);

	/* read the pool header from first pool set file */
	const char *part0_path = PART(REP(set, 0), 0)->path;
	struct pool_hdr hdr;
	if (util_file_pread(part0_path, &hdr, sizeof(hdr), 0) !=
			sizeof(hdr)) {
		outv_err("cannot read pool header from poolset\n");
		goto err_pool_set;
	}

	util_poolset_free(set);

	util_convert2h_hdr_nocheck(&hdr);

	/* parse pool type from first pool set file */
	pmem_pool_type_t type = pmem_pool_type_parse_hdr(&hdr);
	if (type == PMEM_POOL_TYPE_UNKNOWN) {
		outv_err("cannot determine pool type from poolset\n");
		return -1;
	}

	/*
	 * Just use one thread - there is no need for multi-threaded access
	 * to remote pool.
	 */
	unsigned nlanes = 1;

	/*
	 * Open the poolset, the values passed to util_pool_open are read
	 * from the first poolset file, these values are then compared with
	 * the values from all headers of poolset files.
	 */
	struct pool_attr attr;
	util_pool_hdr2attr(&attr, &hdr);
	unsigned flags = (rdonly ? POOL_OPEN_COW : 0) | POOL_OPEN_IGNORE_SDS |
				POOL_OPEN_IGNORE_BAD_BLOCKS;
	if (util_pool_open(poolset, fname, 0 /* minpartsize */,
			&attr, &nlanes, NULL, flags)) {
		outv_err("opening poolset failed\n");
		return -1;
	}

	return 0;

err_pool_set:
	util_poolset_free(set);
	return -1;
}

/*
 * pmem_pool_parse_params -- parse pool type, file size and block size
 */
int
pmem_pool_parse_params(const char *fname, struct pmem_pool_params *paramsp,
		int check)
{
	paramsp->type = PMEM_POOL_TYPE_UNKNOWN;
	char pool_str_addr[POOL_HDR_DESC_SIZE];

	enum file_type type = util_file_get_type(fname);
	if (type < 0)
		return -1;

	int is_poolset = util_is_poolset_file(fname);
	if (is_poolset < 0)
		return -1;

	paramsp->is_poolset = is_poolset;
	int fd = util_file_open(fname, NULL, 0, O_RDONLY);
	if (fd < 0)
		return -1;

	/* get file size and mode */
	os_stat_t stat_buf;
	if (os_fstat(fd, &stat_buf)) {
		os_close(fd);
		return -1;
	}

	int ret = 0;

	assert(stat_buf.st_size >= 0);
	paramsp->size = (uint64_t)stat_buf.st_size;
	paramsp->mode = stat_buf.st_mode;

	void *addr = NULL;
	struct pool_set *set = NULL;
	if (paramsp->is_poolset) {
		/* close the file */
		os_close(fd);
		fd = -1;

		if (check) {
			if (util_poolset_map(fname, &set, 0)) {
				ret = -1;
				goto out_close;
			}
		} else {
			ret = util_poolset_create_set(&set, fname, 0, 0, true);
			if (ret < 0) {
				outv_err("cannot open pool set -- '%s'", fname);
				ret = -1;
				goto out_close;
			}
			if (util_pool_open_nocheck(set,
						POOL_OPEN_IGNORE_BAD_BLOCKS)) {
				ret = -1;
				goto out_close;
			}
		}

		paramsp->size = set->poolsize;
		addr = set->replica[0]->part[0].addr;

		/*
		 * XXX mprotect for device dax with length not aligned to its
		 * page granularity causes SIGBUS on the next page fault.
		 * The length argument of this call should be changed to
		 * set->poolsize once the kernel issue is solved.
		 */
		if (mprotect(addr, set->replica[0]->repsize,
			PROT_READ) < 0) {
			outv_err("!mprotect");
			goto out_close;
		}
	} else {
		/* read first two pages */
		if (type == TYPE_DEVDAX) {
			addr = util_file_map_whole(fname);
			if (addr == NULL) {
				ret = -1;
				goto out_close;
			}
		} else {
			ssize_t num = read(fd, pool_str_addr,
					POOL_HDR_DESC_SIZE);
			if (num < (ssize_t)POOL_HDR_DESC_SIZE) {
				outv_err("!read");
				ret = -1;
				goto out_close;
			}
			addr = pool_str_addr;
		}
	}

	struct pool_hdr hdr;
	memcpy(&hdr, addr, sizeof(hdr));

	util_convert2h_hdr_nocheck(&hdr);

	memcpy(paramsp->signature, hdr.signature, sizeof(paramsp->signature));

	/*
	 * Check if file is a part of pool set by comparing
	 * the UUID with the next part UUID. If it is the same
	 * it means the pool consist of a single file.
	 */
	paramsp->is_part = !paramsp->is_poolset &&
		(memcmp(hdr.uuid, hdr.next_part_uuid, POOL_HDR_UUID_LEN) ||
		memcmp(hdr.uuid, hdr.prev_part_uuid, POOL_HDR_UUID_LEN) ||
		memcmp(hdr.uuid, hdr.next_repl_uuid, POOL_HDR_UUID_LEN) ||
		memcmp(hdr.uuid, hdr.prev_repl_uuid, POOL_HDR_UUID_LEN));

	if (check)
		paramsp->type = pmem_pool_type(addr);
	else
		paramsp->type = pmem_pool_type_parse_hdr(addr);

	paramsp->is_checksum_ok = pmem_pool_checksum(addr);

	if (paramsp->type == PMEM_POOL_TYPE_BLK) {
		struct pmemblk *pbp = addr;
		paramsp->blk.bsize = le32toh(pbp->bsize);
	} else if (paramsp->type == PMEM_POOL_TYPE_OBJ) {
		struct pmemobjpool *pop = addr;
		memcpy(paramsp->obj.layout, pop->layout, PMEMOBJ_MAX_LAYOUT);
	}

	if (paramsp->is_poolset)
		util_poolset_close(set, DO_NOT_DELETE_PARTS);

out_close:
	if (fd >= 0)
		(void) os_close(fd);
	return ret;
}

/*
 * util_check_memory -- check if memory contains single value
 */
int
util_check_memory(const uint8_t *buff, size_t len, uint8_t val)
{
	size_t i;
	for (i = 0; i < len; i++) {
		if (buff[i] != val)
			return -1;
	}

	return 0;
}

/*
 * pmempool_ask_yes_no -- prints the question,
 * takes user answer and returns validated value
 */
static char
pmempool_ask_yes_no(char def_ans, const char *answers, const char *qbuff)
{
	char ret = INV_ANS;
	printf("%s", qbuff);
	size_t len = strlen(answers);
	size_t i;

	char def_anslo = (char)tolower(def_ans);
	printf(" [");
	for (i = 0; i < len; i++) {
		char anslo = (char)tolower(answers[i]);
		printf("%c", anslo == def_anslo ?
				toupper(anslo) : anslo);
		if (i != len - 1)
			printf("/");
	}
	printf("] ");

	char *line_of_answer = util_readline(stdin);

	if (line_of_answer == NULL) {
		outv_err("input is empty");
		return '?';
	}

	char first_letter = line_of_answer[0];
	line_of_answer[0] = (char)tolower(first_letter);

	if (strcmp(line_of_answer, "yes\n") == 0) {
		if (strchr(answers, 'y') != NULL)
			ret = 'y';
	}

	if (strcmp(line_of_answer, "no\n") == 0) {
		if (strchr(answers, 'n') != NULL)
			ret = 'n';
	}

	if (strlen(line_of_answer) == 2 &&
			line_of_answer[1] == '\n') {
		if (strchr(answers, line_of_answer[0]) != NULL)
			ret = line_of_answer[0];
	}

	if (strlen(line_of_answer) == 1 &&
			line_of_answer[0] == '\n') {
		ret = def_ans;
	}

	Free(line_of_answer);
	return ret;
}

/*
 * ask -- keep asking for answer until it gets valid input
 */
char
ask(char op, char *answers, char def_ans, const char *fmt, va_list ap)
{
	char qbuff[Q_BUFF_SIZE];
	char ret = INV_ANS;
	int is_tty = 0;
	if (op != '?')
		return op;

	int p = vsnprintf(qbuff, Q_BUFF_SIZE, fmt, ap);
	if (p < 0) {
		outv_err("vsnprintf");
		exit(EXIT_FAILURE);
	}
	if (p >= Q_BUFF_SIZE) {
		outv_err("vsnprintf: output was truncated");
		exit(EXIT_FAILURE);
	}

	is_tty = isatty(fileno(stdin));

	while ((ret = pmempool_ask_yes_no(def_ans, answers, qbuff)) == INV_ANS)
		;

	if (!is_tty)
		printf("%c\n", ret);

	return ret;
}

char
ask_Yn(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask(op, "yn", 'y', fmt, ap);
	va_end(ap);
	return ret;
}

char
ask_yN(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask(op, "yn", 'n', fmt, ap);
	va_end(ap);
	return ret;
}

/*
 * util_parse_enum -- parse single enum and store to bitmap
 */
static int
util_parse_enum(const char *str, int first, int max, uint64_t *bitmap,
		enum_to_str_fn enum_to_str)
{
	for (int i = first; i < max; i++) {
		if (strcmp(str, enum_to_str(i)) == 0) {
			*bitmap |= (uint64_t)1<<i;
			return 0;
		}
	}

	return -1;
}

/*
 * util_parse_enums -- parse enums and store to bitmap
 */
static int
util_parse_enums(const char *str, int first, int max, uint64_t *bitmap,
		enum_to_str_fn enum_to_str)
{
	char *dup = strdup(str);
	if (!dup)
		err(1, "Cannot allocate memory for enum str");

	char *ptr = dup;
	int ret = 0;
	char *comma;
	do {
		comma = strchr(ptr, ',');
		if (comma) {
			*comma = '\0';
			comma++;
		}

		if ((ret = util_parse_enum(ptr, first, max,
				bitmap, enum_to_str))) {
			goto out;
		}

		ptr = comma;
	} while (ptr);
out:
	free(dup);
	return ret;
}

/*
 * util_parse_chunk_types -- parse chunk types strings
 */
int
util_parse_chunk_types(const char *str, uint64_t *types)
{
	assert(MAX_CHUNK_TYPE < 8 * sizeof(*types));
	return util_parse_enums(str, 0, MAX_CHUNK_TYPE, types,
			(enum_to_str_fn)out_get_chunk_type_str);
}

/*
 * util_options_alloc -- allocate and initialize options structure
 */
struct options *
util_options_alloc(const struct option *options,
		size_t nopts, const struct option_requirement *req)
{
	struct options *opts = calloc(1, sizeof(*opts));
	if (!opts)
		err(1, "Cannot allocate memory for options structure");

	opts->opts = options;
	opts->noptions = nopts;
	opts->req = req;
	size_t bitmap_size = howmany(nopts, 8);
	opts->bitmap = calloc(bitmap_size, 1);
	if (!opts->bitmap)
		err(1, "Cannot allocate memory for options bitmap");

	return opts;
}

/*
 * util_options_free -- free options structure
 */
void
util_options_free(struct options *opts)
{
	free(opts->bitmap);
	free(opts);
}

/*
 * util_opt_get_index -- return index of specified option in global
 * array of options
 */
static int
util_opt_get_index(const struct options *opts, int opt)
{
	const struct option *lopt = &opts->opts[0];
	int ret = 0;
	while (lopt->name) {
		if ((lopt->val & ~OPT_MASK) == opt)
			return ret;
		lopt++;
		ret++;
	}
	return -1;
}

/*
 * util_opt_get_req -- get required option for specified option
 */
static struct option_requirement *
util_opt_get_req(const struct options *opts, int opt, pmem_pool_type_t type)
{
	size_t n = 0;
	struct option_requirement *ret = NULL;
	struct option_requirement *tmp = NULL;
	const struct option_requirement *req = &opts->req[0];
	while (req->opt) {
		if (req->opt == opt && (req->type & type)) {
			n++;
			tmp = realloc(ret, n * sizeof(*ret));
			if (!tmp)
				err(1, "Cannot allocate memory for"
					" option requirements");
			ret = tmp;
			ret[n - 1] = *req;
		}
		req++;
	}

	if (ret) {
		tmp = realloc(ret, (n + 1) * sizeof(*ret));
		if (!tmp)
			err(1, "Cannot allocate memory for"
				" option requirements");
		ret = tmp;
		memset(&ret[n], 0, sizeof(*ret));
	}

	return ret;
}

/*
 * util_opt_check_requirements -- check if requirements has been fulfilled
 */
static int
util_opt_check_requirements(const struct options *opts,
		const struct option_requirement *req)
{
	int count = 0;
	int set = 0;
	uint64_t tmp;
	while ((tmp = req->req) != 0) {
		while (tmp) {
			int req_idx =
				util_opt_get_index(opts, tmp & OPT_REQ_MASK);

			if (req_idx >= 0 && util_isset(opts->bitmap, req_idx)) {
				set++;
				break;
			}

			tmp >>= OPT_REQ_SHIFT;
		}
		req++;
		count++;
	}

	return count != set;
}

/*
 * util_opt_print_requirements -- print requirements for specified option
 */
static void
util_opt_print_requirements(const struct options *opts,
		const struct option_requirement *req)
{
	char buff[REQ_BUFF_SIZE];
	unsigned n = 0;
	uint64_t tmp;
	const struct option *opt =
		&opts->opts[util_opt_get_index(opts, req->opt)];
	int sn;

	sn = util_snprintf(&buff[n], REQ_BUFF_SIZE - n,
			"option [-%c|--%s] requires: ", opt->val, opt->name);
	assert(sn >= 0);
	if (sn >= 0)
		n += (unsigned)sn;

	size_t rc = 0;
	while ((tmp = req->req) != 0) {
		if (rc != 0) {
			sn = util_snprintf(&buff[n], REQ_BUFF_SIZE - n,
					" and ");
			assert(sn >= 0);
			if (sn >= 0)
				n += (unsigned)sn;
		}

		size_t c = 0;
		while (tmp) {
			sn = util_snprintf(&buff[n], REQ_BUFF_SIZE - n,
					c == 0 ? "[" : "|");
			assert(sn >= 0);
			if (sn >= 0)
				n += (unsigned)sn;

			int req_opt_ind =
				util_opt_get_index(opts, tmp & OPT_REQ_MASK);
			const struct option *req_option =
				&opts->opts[req_opt_ind];

			sn = util_snprintf(&buff[n], REQ_BUFF_SIZE - n,
				"-%c|--%s", req_option->val, req_option->name);
			assert(sn >= 0);
			if (sn >= 0)
				n += (unsigned)sn;

			tmp >>= OPT_REQ_SHIFT;
			c++;
		}
		sn = util_snprintf(&buff[n], REQ_BUFF_SIZE - n, "]");
		assert(sn >= 0);
		if (sn >= 0)
			n += (unsigned)sn;

		req++;
		rc++;
	}

	outv_err("%s\n", buff);
}

/*
 * util_opt_verify_requirements -- verify specified requirements for options
 */
static int
util_opt_verify_requirements(const struct options *opts, size_t index,
		pmem_pool_type_t type)
{
	const struct option *opt = &opts->opts[index];
	int val = opt->val & ~OPT_MASK;
	struct option_requirement *req;

	if ((req = util_opt_get_req(opts, val, type)) == NULL)
		return 0;

	int ret = 0;

	if (util_opt_check_requirements(opts, req)) {
		ret = -1;
		util_opt_print_requirements(opts, req);
	}

	free(req);
	return ret;
}

/*
 * util_opt_verify_type -- check if used option matches pool type
 */
static int
util_opt_verify_type(const struct options *opts, pmem_pool_type_t type,
		size_t index)
{
	const struct option *opt = &opts->opts[index];
	int val = opt->val & ~OPT_MASK;
	int opt_type = opt->val;
	opt_type >>= OPT_SHIFT;
	if (!(opt_type & (1<<type))) {
		outv_err("'--%s|-%c' -- invalid option specified"
			" for pool type '%s'\n",
			opt->name, val,
			out_get_pool_type_str(type));
		return -1;
	}

	return 0;
}

/*
 * util_options_getopt -- wrapper for getopt_long which sets bitmap
 */
int
util_options_getopt(int argc, char *argv[], const char *optstr,
		const struct options *opts)
{
	int opt = getopt_long(argc, argv, optstr, opts->opts, NULL);
	if (opt == -1 || opt == '?')
		return opt;

	opt &= ~OPT_MASK;
	int option_index = util_opt_get_index(opts, opt);
	assert(option_index >= 0);

	util_setbit((uint8_t *)opts->bitmap, (unsigned)option_index);

	return opt;
}

/*
 * util_options_verify -- verify options
 */
int
util_options_verify(const struct options *opts, pmem_pool_type_t type)
{
	for (size_t i = 0; i < opts->noptions; i++) {
		if (util_isset(opts->bitmap, i)) {
			if (util_opt_verify_type(opts, type, i))
				return -1;

			if (opts->req)
				if (util_opt_verify_requirements(opts, i, type))
					return -1;
		}
	}

	return 0;
}

/*
 * util_heap_max_zone -- get number of zones
 */
unsigned
util_heap_max_zone(size_t size)
{
	unsigned max_zone = 0;
	size -= sizeof(struct heap_header);

	while (size >= ZONE_MIN_SIZE) {
		max_zone++;
		size -= size <= ZONE_MAX_SIZE ? size : ZONE_MAX_SIZE;
	}

	return max_zone;
}

/*
 * pool_set_file_open -- opens pool set file or regular file
 */
struct pool_set_file *
pool_set_file_open(const char *fname,
		int rdonly, int check)
{
	struct pool_set_file *file = calloc(1, sizeof(*file));
	if (!file)
		return NULL;

	file->replica = 0;
	file->fname = strdup(fname);
	if (!file->fname)
		goto err;

	os_stat_t buf;
	if (os_stat(fname, &buf)) {
		warn("%s", fname);
		goto err_free_fname;
	}

	file->mtime = buf.st_mtime;
	file->mode = buf.st_mode;
	if (S_ISBLK(file->mode))
		file->fileio = true;

	if (file->fileio) {
		/* Simple file open for BTT device */
		int fd = util_file_open(fname, NULL, 0, O_RDONLY);
		if (fd < 0) {
			outv_err("util_file_open failed\n");
			goto err_free_fname;
		}

		os_off_t seek_size = os_lseek(fd, 0, SEEK_END);
		if (seek_size == -1) {
			outv_err("lseek SEEK_END failed\n");
			os_close(fd);
			goto err_free_fname;
		}

		file->size = (size_t)seek_size;
		file->fd = fd;
	} else {
		/*
		 * The check flag indicates whether the headers from each pool
		 * set file part should be checked for valid values.
		 */
		if (check) {
			if (util_poolset_map(file->fname,
					&file->poolset, rdonly))
				goto err_free_fname;
		} else {
			int ret = util_poolset_create_set(&file->poolset,
				file->fname, 0, 0, true);

			if (ret < 0) {
				outv_err("cannot open pool set -- '%s'",
					file->fname);
				goto err_free_fname;
			}
			unsigned flags = (rdonly ? POOL_OPEN_COW : 0) |
						POOL_OPEN_IGNORE_BAD_BLOCKS;
			if (util_pool_open_nocheck(file->poolset, flags))
				goto err_free_fname;
		}

		/* get modification time from the first part of first replica */
		const char *path = file->poolset->replica[0]->part[0].path;
		if (os_stat(path, &buf)) {
			warn("%s", path);
			goto err_close_poolset;
		}
		file->size = file->poolset->poolsize;
		file->addr = file->poolset->replica[0]->part[0].addr;
	}
	return file;

err_close_poolset:
	util_poolset_close(file->poolset, DO_NOT_DELETE_PARTS);
err_free_fname:
	free(file->fname);
err:
	free(file);
	return NULL;
}

/*
 * pool_set_file_close -- closes pool set file or regular file
 */
void
pool_set_file_close(struct pool_set_file *file)
{
	if (!file->fileio) {
		if (file->poolset)
			util_poolset_close(file->poolset, DO_NOT_DELETE_PARTS);
		else if (file->addr) {
			munmap(file->addr, file->size);
			os_close(file->fd);
		}
	}
	free(file->fname);
	free(file);
}

/*
 * pool_set_file_read -- read from pool set file or regular file
 *
 * 'buff' has to be a buffer at least 'nbytes' long
 * 'off' is an offset from the beginning of the file
 */
int
pool_set_file_read(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off)
{
	if (off + nbytes > file->size)
		return -1;

	if (file->fileio) {
		ssize_t num = pread(file->fd, buff, nbytes, (os_off_t)off);
		if (num < (ssize_t)nbytes)
			return -1;
	} else {
		memcpy(buff, (char *)file->addr + off, nbytes);
	}
	return 0;
}

/*
 * pool_set_file_write -- write to pool set file or regular file
 *
 * 'buff' has to be a buffer at least 'nbytes' long
 * 'off' is an offset from the beginning of the file
 */
int
pool_set_file_write(struct pool_set_file *file, void *buff,
		size_t nbytes, uint64_t off)
{
	enum file_type type = util_file_get_type(file->fname);
	if (type < 0)
		return -1;

	if (off + nbytes > file->size)
		return -1;

	if (file->fileio) {
		ssize_t num = pwrite(file->fd, buff, nbytes, (os_off_t)off);
		if (num < (ssize_t)nbytes)
			return -1;
	} else {
		memcpy((char *)file->addr + off, buff, nbytes);
		util_persist_auto(type == TYPE_DEVDAX, (char *)file->addr + off,
					nbytes);
	}
	return 0;
}

/*
 * pool_set_file_set_replica -- change replica for pool set file
 */
int
pool_set_file_set_replica(struct pool_set_file *file, size_t replica)
{
	if (!replica)
		return 0;

	if (!file->poolset)
		return -1;

	if (replica >= file->poolset->nreplicas)
		return -1;

	if (file->poolset->replica[replica]->remote) {
		outv_err("reading from remote replica not supported");
		return -1;
	}

	file->replica = replica;
	file->addr = file->poolset->replica[replica]->part[0].addr;

	return 0;
}

/*
 * pool_set_file_nreplicas -- return number of replicas
 */
size_t
pool_set_file_nreplicas(struct pool_set_file *file)
{
	return file->poolset->nreplicas;
}

/*
 * pool_set_file_map -- return mapped address at given offset
 */
void *
pool_set_file_map(struct pool_set_file *file, uint64_t offset)
{
	if (file->addr == MAP_FAILED)
		return NULL;
	return (char *)file->addr + offset;
}

/*
 * pool_set_file_persist -- propagates and persists changes to a memory range
 *
 * 'addr' points to the beginning of data in the master replica that has to be
 *        propagated
 * 'len' is the number of bytes to be propagated to other replicas
 */
void
pool_set_file_persist(struct pool_set_file *file, const void *addr, size_t len)
{
	uintptr_t offset = (uintptr_t)((char *)addr -
		(char *)file->poolset->replica[0]->part[0].addr);

	for (unsigned r = 1; r < file->poolset->nreplicas; ++r) {
		struct pool_replica *rep = file->poolset->replica[r];
		void *dst = (char *)rep->part[0].addr + offset;
		memcpy(dst, addr, len);
		util_persist(rep->is_pmem, dst, len);
	}
	struct pool_replica *rep = file->poolset->replica[0];
	util_persist(rep->is_pmem, (void *)addr, len);
}

/*
 * util_pool_clear_badblocks -- clear badblocks in a pool (set or a single file)
 */
int
util_pool_clear_badblocks(const char *path, int create)
{
	LOG(3, "path %s create %i", path, create);

	struct pool_set *setp;

	/* do not check minsize */
	int ret = util_poolset_create_set(&setp, path, 0, 0,
						POOL_OPEN_IGNORE_SDS);
	if (ret < 0) {
		LOG(2, "cannot open pool set -- '%s'", path);
		return -1;
	}

	if (badblocks_clear_poolset(setp, create)) {
		outv_err("clearing bad blocks in the pool set failed -- '%s'",
			path);
		errno = EIO;
		return -1;
	}

	return 0;
}
