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
#include <ctype.h>
#include "common.h"
#include "libpmemblk.h"
#include "libpmemlog.h"

#define	__USE_UNIX98
#include <unistd.h>

/*
 * pmem_pool_type_parse -- return pool type based on pool header data
 */
pmem_pool_type_t
pmem_pool_type_parse_hdr(struct pool_hdr *hdrp)
{
	if (strncmp(hdrp->signature, LOG_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_LOG;
	else if (strncmp(hdrp->signature, BLK_HDR_SIG, POOL_HDR_SIG_LEN) == 0)
		return PMEM_POOL_TYPE_BLK;
	else
		return PMEM_POOL_TYPE_UNKNWON;
}

/*
 * pmempool_info_parse_type -- returns pool type from command line arg
 */
pmem_pool_type_t
pmem_pool_type_parse_str(char *str)
{
	if (strcmp(str, "blk") == 0) {
		return PMEM_POOL_TYPE_BLK;
	} else if (strcmp(str, "log") == 0) {
		return PMEM_POOL_TYPE_LOG;
	} else {
		return PMEM_POOL_TYPE_UNKNWON;
	}
}

/*
 * util_validate_checksum -- validate checksum and return valid one
 */
int
util_validate_checksum(void *addr, size_t len, uint64_t *csum)
{
	/* validate checksum */
	int csum_valid = util_checksum(addr, len, csum, 0);
	/* get valid one */
	if (!csum_valid)
		util_checksum(addr, len, csum, 1);
	return csum_valid;
}

/*
 * util_parse_size -- parse size from string
 */
int
util_parse_size(char *str, uint64_t *sizep)
{
	uint64_t size = 0;
	int shift = 0;
	char unit[3] = {0};
	int ret = sscanf(str, "%lu%3s", &size, unit);
	if (ret <= 0)
		return -1;
	if (ret == 2) {
		if ((unit[1] != '\0' && unit[1] != 'B') ||
			unit[2] != '\0')
			return -1;
		switch (unit[0]) {
		case 'K':
			shift = 10;
			break;
		case 'M':
			shift = 20;
			break;
		case 'G':
			shift = 30;
			break;
		case 'T':
			shift = 40;
			break;
		case 'P':
			shift = 50;
			break;
		default:
			return -1;
		}
	}

	if (sizep)
		*sizep = size << shift;

	return 0;
}

/*
 * util_parse_mode -- parse file mode from octal string
 */
int
util_parse_mode(char *str, mode_t *mode)
{
	int digits = 0;
	int zero = *str == '0';
	*mode = 0;
	while (*str != '\0') {
		if (digits == 3)
			return -1;
		if (*str < '0' || *str > '7')
			return -1;
		if (digits || *str != '0') {
			*mode = (*mode << 3) | (*str - '0');
			digits++;
		}
		str++;
	}

	return digits || zero ? 0 : -1;
}

/*
 * util_parse_range_from_to -- parse range string as interval
 */
static int
util_parse_range_from_to(char *str, struct range *rangep, struct range *entirep)
{
	char *str1 = NULL;
	char sep = '\0';
	char *str2 = NULL;

	int ret = 0;
	if (sscanf(str, "%m[^-]%c%m[^-]", &str1, &sep, &str2) == 3 &&
			sep == '-') {
		if (util_parse_size(str1, &rangep->first) != 0)
			ret = -1;
		else if (util_parse_size(str2, &rangep->last) != 0)
			ret = -1;
		if (rangep->first > entirep->last ||
		    rangep->last > entirep->last)
			return -1;
		if (rangep->first > rangep->last) {
			uint64_t tmp = rangep->first;
			rangep->first = rangep->last;
			rangep->last = tmp;
		}
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);
	if (str2)
		free(str2);

	return ret;
}

/*
 * util_parse_range_from -- parse range string as interval from specified number
 */
static int
util_parse_range_from(char *str, struct range *rangep, struct range *entirep)
{
	char *str1 = NULL;
	char sep = '\0';

	int ret = 0;
	if (sscanf(str, "%m[^-]%c", &str1, &sep) == 2 && sep == '-') {
		if (util_parse_size(str1, &rangep->first) == 0)
			rangep->last = entirep->last;
		else
			ret = -1;
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);

	return ret;
}

/*
 * util_parse_range_to -- parse range string as interval to specified number
 */
static int
util_parse_range_to(char *str, struct range *rangep, struct range *entirep)
{
	char *str1 = NULL;
	char sep = '\0';

	int ret = 0;
	if (sscanf(str, "%c%m[^-]", &sep, &str1) == 2 && sep == '-') {
		if (util_parse_size(str1, &rangep->last) == 0) {
			rangep->first = entirep->first;
			if (rangep->last > entirep->last)
				return -1;
		} else {
			ret = -1;
		}
	} else {
		ret = -1;
	}

	if (str1)
		free(str1);

	return ret;
}

/*
 * util_parse_range_number -- parse range string as a single number
 */
static int
util_parse_range_number(char *str, struct range *rangep, struct range *entirep)
{
	util_parse_size(str, &rangep->first);
	if (rangep->first > entirep->last ||
	    rangep->last > entirep->last)
		return -1;
	rangep->last = rangep->first;
	return 0;
}

/*
 * util_parse_range -- parse single range string
 */
static int
util_parse_range(char *str, struct range *rangep, struct range *entirep)
{
	if (util_parse_range_from_to(str, rangep, entirep) == 0)
		return 0;
	if (util_parse_range_from(str, rangep, entirep) == 0)
		return 0;
	if (util_parse_range_to(str, rangep, entirep) == 0)
		return 0;
	if (util_parse_range_number(str, rangep, entirep) == 0)
		return 0;
	return -1;
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
 * util_ranges_add_range -- merge overlapping ranges and add to list
 */
static int
util_ranges_add_range(struct ranges *rangesp, struct range *rangep)
{
	struct range *curp  = NULL;
	uint64_t first = rangep->first;
	uint64_t last = rangep->last;
	LIST_FOREACH(curp, &rangesp->head, next) {
		if (util_ranges_overlap(curp, rangep)) {
			LIST_REMOVE(curp, next);
			if (curp->first < first)
				first = curp->first;
			if (curp->last > last)
				last = curp->last;
			free(curp);
		}
	}

	rangep->first = first;
	rangep->last = last;

	LIST_FOREACH(curp, &rangesp->head, next) {
		if (curp->first < rangep->first) {
			LIST_INSERT_AFTER(curp, rangep, next);
			return 0;
		}
	}

	LIST_INSERT_HEAD(&rangesp->head, rangep, next);

	return 0;
}

/*
 * util_ranges_add -- create and add range
 */
int
util_ranges_add(struct ranges *rangesp, uint64_t first, uint64_t last)
{
	struct range *rangep = malloc(sizeof (struct range));
	if (!rangep)
		err(1, "Cannot allocate memory for range\n");
	rangep->first = first;
	rangep->last = last;
	return util_ranges_add_range(rangesp, rangep);
}

/*
 * util_ranges_clear -- clear list of ranges
 */
void
util_ranges_clear(struct ranges *rangesp)
{
	while (!LIST_EMPTY(&rangesp->head)) {
		struct range *rangep = LIST_FIRST(&rangesp->head);
		LIST_REMOVE(rangep, next);
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
util_parse_ranges(char *str, struct ranges *rangesp, struct range *entirep)
{
	char *next = str;
	do {
		str = next;
		next = strchr(str, ',');
		if (next != NULL) {
			*next = '\0';
			next++;
		}
		struct range *rangep = malloc(sizeof (struct range));
		if (!rangep)
			err(1, "Cannot allocate memory for range\n");

		if (util_parse_range(str, rangep, entirep)) {
			free(rangep);
			return -1;
		} else {
			if (util_ranges_add_range(rangesp, rangep)) {
				free(rangep);
				return -1;
			}
		}
	} while (next != NULL);

	return 0;
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
	default:
		break;
	}

	return 0;
}

/*
 * pmem_pool_parse_params -- parse pool type, file size and block size
 */
pmem_pool_type_t
pmem_pool_parse_params(char *fname, uint64_t *sizep, uint64_t *bsizep)
{
	pmem_pool_type_t ret = PMEM_POOL_TYPE_NONE;
	struct stat stat_buf;

	/* use pmemblk as wee need pool_hdr only or pmemblk */
	struct pmemblk *pbp = malloc(sizeof (struct pmemblk));
	if (!pbp)
		err(1, "Cannot allocate memory for pmemblk structure\n");

	int fd;
	if ((fd = open(fname, O_RDONLY)) >= 0) {
		ret = PMEM_POOL_TYPE_UNKNWON;
		/* read pool_hdr */
		if (pread(fd, &pbp->hdr, sizeof (pbp->hdr), 0) ==
				sizeof (pbp->hdr))
			ret = pmem_pool_type_parse_hdr(&pbp->hdr);
		if (ret == PMEM_POOL_TYPE_BLK) {
			/*
			 * For pmem blk pool type we need block size,
			 * so read pmemblk struct.
			 */
			if (bsizep && pread(fd, pbp, sizeof (*pbp), 0) ==
				sizeof (*pbp))
				*bsizep = le32toh(pbp->bsize);
		}

		/* get file size */
		if (sizep && fstat(fd, &stat_buf) == 0)
			*sizep = stat_buf.st_size;

		close(fd);
	}

	free(pbp);
	return ret;
}

/*
 * util_pool_hdr_convert -- convert pool header to host byte order
 */
void
util_convert2h_pool_hdr(struct pool_hdr *hdrp)
{
	hdrp->compat_features = le32toh(hdrp->compat_features);
	hdrp->incompat_features = le32toh(hdrp->incompat_features);
	hdrp->ro_compat_features = le32toh(hdrp->ro_compat_features);
	hdrp->crtime = le64toh(hdrp->crtime);
	hdrp->checksum = le64toh(hdrp->checksum);
}

/*
 * util_pool_hdr_convert -- convert pool header to LE byte order
 */
void
util_convert2le_pool_hdr(struct pool_hdr *hdrp)
{
	hdrp->compat_features = htole32(hdrp->compat_features);
	hdrp->incompat_features = htole32(hdrp->incompat_features);
	hdrp->ro_compat_features = htole32(hdrp->ro_compat_features);
	hdrp->crtime = htole64(hdrp->crtime);
	hdrp->checksum = htole64(hdrp->checksum);
}

/*
 * util_convert_btt_info -- convert btt_info header to host byte order
 */
void
util_convert2h_btt_info(struct btt_info *infop)
{
	infop->flags = le32toh(infop->flags);
	infop->minor = le16toh(infop->minor);
	infop->external_lbasize = le32toh(infop->external_lbasize);
	infop->external_nlba = le32toh(infop->external_nlba);
	infop->internal_lbasize = le32toh(infop->internal_lbasize);
	infop->internal_nlba = le32toh(infop->internal_nlba);
	infop->nfree = le32toh(infop->nfree);
	infop->infosize = le32toh(infop->infosize);
	infop->nextoff = le64toh(infop->nextoff);
	infop->dataoff = le64toh(infop->dataoff);
	infop->mapoff = le64toh(infop->mapoff);
	infop->flogoff = le64toh(infop->flogoff);
	infop->infooff = le64toh(infop->infooff);
	infop->checksum = le64toh(infop->checksum);
}

/*
 * util_convert_btt_info -- convert btt_info header to LE byte order
 */
void
util_convert2le_btt_info(struct btt_info *infop)
{
	infop->flags = htole64(infop->flags);
	infop->minor = htole16(infop->minor);
	infop->external_lbasize = htole32(infop->external_lbasize);
	infop->external_nlba = htole32(infop->external_nlba);
	infop->internal_lbasize = htole32(infop->internal_lbasize);
	infop->internal_nlba = htole32(infop->internal_nlba);
	infop->nfree = htole32(infop->nfree);
	infop->infosize = htole32(infop->infosize);
	infop->nextoff = htole64(infop->nextoff);
	infop->dataoff = htole64(infop->dataoff);
	infop->mapoff = htole64(infop->mapoff);
	infop->flogoff = htole64(infop->flogoff);
	infop->infooff = htole64(infop->infooff);
	infop->checksum = htole64(infop->checksum);
}

/*
 * util_convert2h_btt_flog -- convert btt_flog to host byte order
 */
void
util_convert2h_btt_flog(struct btt_flog *flogp)
{
	flogp->lba = le32toh(flogp->lba);
	flogp->old_map = le32toh(flogp->old_map);
	flogp->new_map = le32toh(flogp->new_map);
	flogp->seq = le32toh(flogp->seq);
}

/*
 * util_convert2le_btt_flog -- convert btt_flog to LE byte order
 */
void
util_convert2le_btt_flog(struct btt_flog *flogp)
{
	flogp->lba = htole32(flogp->lba);
	flogp->old_map = htole32(flogp->old_map);
	flogp->new_map = htole32(flogp->new_map);
	flogp->seq = htole32(flogp->seq);
}

/*
 * util_convert2h_pmemlog -- convert pmemlog structure to host byte order
 */
void
util_convert2h_pmemlog(struct pmemlog *plp)
{
	plp->start_offset = le64toh(plp->start_offset);
	plp->end_offset = le64toh(plp->end_offset);
	plp->write_offset = le64toh(plp->write_offset);
}

/*
 * util_convert2le_pmemlog -- convert pmemlog structure to LE byte order
 */
void
util_convert2le_pmemlog(struct pmemlog *plp)
{
	plp->start_offset = htole64(plp->start_offset);
	plp->end_offset = htole64(plp->end_offset);
	plp->write_offset = htole64(plp->write_offset);
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
 * util_get_max_bsize -- return maximum size of block for given file size
 */
uint32_t
util_get_max_bsize(uint64_t fsize)
{
	if (fsize == 0)
		return 0;

	/* default nfree */
	uint32_t nfree = BTT_DEFAULT_NFREE;

	/* number of blocks must be at least 2 * nfree */
	uint32_t internal_nlba = 2 * nfree;

	/* compute flog size */
	int flog_size = nfree *
		roundup(2 * sizeof (struct btt_flog), BTT_FLOG_PAIR_ALIGN);
	flog_size = roundup(flog_size, BTT_ALIGNMENT);

	/* compute arena size from file size */
	uint64_t arena_size = fsize;
	/* without pmemblk structure */
	arena_size -= sizeof (struct pmemblk);
	if (arena_size > BTT_MAX_ARENA) {
		arena_size = BTT_MAX_ARENA;
	}
	/* without BTT Info header and backup */
	arena_size -= 2 * sizeof (struct btt_info);
	/* without BTT FLOG size */
	arena_size -= flog_size;

	/* compute maximum internal LBA size */
	uint32_t internal_lbasize = (arena_size - BTT_ALIGNMENT) /
			internal_nlba - BTT_MAP_ENTRY_SIZE;

	if (internal_lbasize < BTT_MIN_LBA_SIZE)
		internal_lbasize = BTT_MIN_LBA_SIZE;

	internal_lbasize =
		roundup(internal_lbasize, BTT_INTERNAL_LBA_ALIGNMENT)
			- BTT_INTERNAL_LBA_ALIGNMENT;

	return internal_lbasize;
}

/*
 * util_check_bsize -- check if block size is valid for given file size
 */
int
util_check_bsize(uint32_t bsize, uint64_t fsize)
{
	uint32_t max_bsize = util_get_max_bsize(fsize);
	return !(bsize < max_bsize);
}

char
ask(char op, char *answers, char def_ans, const char *fmt, va_list ap)
{
	char ans = '\0';
	if (op != '?')
		return op;
	do {
		vprintf(fmt, ap);
		size_t len = strlen(answers);
		size_t i;
		char def_ansl = tolower(def_ans);
		printf(" [");
		for (i = 0; i < len; i++) {
			char ans = tolower(answers[i]);
			printf("%c", ans == def_ansl ? toupper(ans) : ans);
			if (i != len -1)
				printf("/");
		}
		printf("] ");
		ans = tolower(getchar());
		if (ans != '\n')
			getchar();
	} while (ans != '\n' && strchr(answers, ans) == NULL);
	return ans == '\n' ? def_ans : ans;
}

char
ask_yn(char op, char def_ans, const char *fmt, va_list ap)
{
	char ret = ask(op, "yn", def_ans, fmt, ap);
	return ret;
}

char
ask_Yn(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask_yn(op, 'y', fmt, ap);
	va_end(ap);
	return ret;
}

char
ask_yN(char op, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	char ret = ask_yn(op, 'n', fmt, ap);
	va_end(ap);
	return ret;
}
