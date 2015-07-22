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
 * spoil.c -- pmempool spoil command source file
 */
#include <features.h>
#define	__USE_UNIX98
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sys/param.h>
#include <libgen.h>
#include <err.h>
#include "common.h"
#include "output.h"

#define	STR(x)	#x

/*
 * Set of macros for parsing structures and fields.
 *
 * Example:
 *
 * PROCESS_BEGIN(psp, pfp) {
 *	PARSE_FIELD(my_struct, my_field, uint32_t);
 *	PARSE(struct_name, arg, max_index)
 * } PROCESS_END
 *
 * return PROCESS_RET
 *
 * The PROCESS_STATE holds the state of processing.
 * The PROCESS_INDEX holds the index of current field.
 */

/*
 * State of processing fields.
 */
enum process_state {
	PROCESS_STATE_NOT_FOUND,
	PROCESS_STATE_FOUND,
	PROCESS_STATE_FIELD,
	PROCESS_STATE_ERROR_MSG,
	PROCESS_STATE_ERROR,
};

#define	PROCESS_BEGIN(psp, pfp) \
enum process_state PROCESS_STATE = PROCESS_STATE_NOT_FOUND;\
struct pmemspoil *_psp = (psp);\
struct pmemspoil_list *_pfp = (pfp);\

#define	PROCESS_RET ((PROCESS_STATE == PROCESS_STATE_FOUND ||\
			PROCESS_STATE == PROCESS_STATE_FIELD) ? 0 : -1)

#define	PROCESS_INDEX	(_pfp->cur->index)

#define	PROCESS_END \
_process_end:\
switch (PROCESS_STATE) {\
case PROCESS_STATE_NOT_FOUND:\
	out_err("unknown field '%s'\n", _pfp->cur->name);\
	break;\
case PROCESS_STATE_FIELD:\
	outv(2, "spoil: %s\n", _pfp->str);\
	break;\
case PROCESS_STATE_ERROR_MSG:\
	out_err("processing '%s'\n", _pfp->str);\
	PROCESS_STATE = PROCESS_STATE_ERROR;\
	break;\
default:\
	break;\
}

#define	PROCESS(_name, _arg, _max) do {\
if (pmemspoil_check_field(_pfp, STR(_name))) {\
	PROCESS_STATE = PROCESS_STATE_FOUND;\
	if (_pfp->cur->index >= (_max)) {\
		PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
	} else {\
		typeof(_arg) a = _arg;\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_name(_psp, _pfp, a))\
			PROCESS_STATE = PROCESS_STATE_ERROR;\
	}\
	goto _process_end;\
}\
} while (0)

#define	PROCESS_FIELD(_ptr, _name, _type) do {\
	if (pmemspoil_check_field(_pfp, STR(_name))) {\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_type(_psp, _pfp,\
				(_type *)&((_ptr)->_name),\
				sizeof ((_ptr)->_name)))\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		else\
			PROCESS_STATE = PROCESS_STATE_FIELD;\
		goto _process_end;\
	}\
} while (0)

/*
 * struct field -- single field with name and id
 */
struct field {
	struct field *next;
	struct field *prev;
	char *name;
	uint64_t index;
};

/*
 * struct pmemspoil_list -- all fields and value
 */
struct pmemspoil_list {
	struct field *head;
	struct field *tail;
	struct field *cur;
	char *value;
	char *str;
};

/*
 * struct pmemspoil -- context and args
 */
struct pmemspoil {
	int verbose;
	char *fname;
	int fd;
	struct pmemspoil_list *args;
	int argc;

};

/*
 * pmemspoil_default -- default context and args
 */
static const struct pmemspoil pmemspoil_default = {
	.verbose	= 1,
	.fname		= NULL,
	.fd		= -1,
	.args		= NULL,
	.argc		= 0,
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Common options:\n"
"  -v, --verbose        Increase verbose level\n"
"  -?, --help           Display this help and exit\n"
"\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"verbose",	no_argument,		0,	'v'},
	{"help",	no_argument,		0,	'?'},
	{0,		0,			0,	 0 },
};

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s <file> <field>=<value>\n", appname);
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
 * pmempool_check_help -- print help message for check command
 */
void
pmemspoil_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmemspoil_parse_field -- parse field name and id from str
 */
static char *
pmemspoil_parse_field(char *str, struct field *fieldp)
{
	if (!str)
		return NULL;

	char *f = strchr(str, '.');
	if (!f)
		f = strchr(str, '=');
	fieldp->index = 0;
	fieldp->name = NULL;
	if (f) {
		*f = '\0';
		size_t len = 0;
		ssize_t ret;
		char *secstr = NULL;
		uint32_t secind;
		/* search for pattern: <field_name>(<index>) */
		if ((ret = sscanf(str, "%m[^\(](%d)", &secstr, &secind) == 2)) {
			len = strlen(secstr);
			str[len] = '\0';
			fieldp->index = secind;
		}

		fieldp->name = str;

		if (secstr)
			free(secstr);

		return f + 1;
	}

	return NULL;
}

/*
 * pmemspoil_free_fields -- free all fields
 */
static void
pmemspoil_free_fields(struct pmemspoil_list *fieldp)
{
	struct field *cur = fieldp->head;
	while (cur != NULL) {
		struct field *next = cur->next;
		free(cur);
		cur = next;
	}

	free(fieldp->str);
}

/*
 * pmemspoil_insert_field -- insert field
 */
static void
pmemspoil_insert_field(struct pmemspoil_list *listp, struct field *fieldp)
{
	fieldp->next = NULL;
	fieldp->prev = NULL;
	if (listp->head == NULL) {
		listp->head = fieldp;
		listp->tail = fieldp;
	} else {
		listp->tail->next = fieldp;
		fieldp->prev = listp->tail;
		listp->tail = fieldp;
	}
}

/*
 * pmemspoil_parse_fields -- parse fields and value from str
 */
static int
pmemspoil_parse_fields(char *str, struct pmemspoil_list *listp)
{
	struct field f;
	char *nstr = NULL;
	listp->str = strdup(str);
	if (!listp->str)
		return -1;
	while ((nstr = pmemspoil_parse_field(str, &f)) != NULL) {
		struct field *fp = malloc(sizeof (struct field));
		if (!fp) {
			pmemspoil_free_fields(listp);
			err(1, NULL);
		}
		memcpy(fp, &f, sizeof (*fp));
		pmemspoil_insert_field(listp, fp);
		str = nstr;
	}

	listp->value = str;
	listp->cur = listp->head;

	return (listp->cur == NULL || listp->value == NULL);
}

/*
 * pmempool_check_parse_args -- parse command line args
 */
static int
pmemspoil_parse_args(struct pmemspoil *psp, char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "v?rNb::q",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'v':
			psp->verbose = 2;
			break;
		case '?':
			pmemspoil_help(appname);
			exit(EXIT_SUCCESS);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		int ind = optind;
		psp->fname = argv[ind];
		ind++;

		psp->argc = (argc - ind);
		psp->args = calloc(psp->argc *
					sizeof (struct pmemspoil_list), 1);
		if (!psp->args)
			err(1, NULL);
		int i;
		for (i = 0; i < psp->argc; i++) {
			char *str = argv[ind];
			if (pmemspoil_parse_fields(str, &psp->args[i])) {
				out_err("ivalid argument");
				exit(EXIT_FAILURE);
			}

			ind += 1;
		}

	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmemspoil_get_arena_offset -- get offset to arena of given id
 */
static uint64_t
pmemspoil_get_arena_offset(struct pmemspoil *psp, uint32_t id)
{
	struct btt_info *infop = calloc(sizeof (struct btt_info), 1);
	if (!infop)
		err(1, NULL);

	infop->nextoff = 2 * BTT_ALIGNMENT;

	off_t offset = 0;
	ssize_t ret = 0;
	id++;
	while (id > 0) {
		if (infop->nextoff == 0) {
			free(infop);
			return 0;
		}
		offset = offset + infop->nextoff;
		if ((ret = pread(psp->fd, infop, sizeof (*infop), offset))
			!= sizeof (*infop)) {
			free(infop);
			return 0;
		}

		util_convert2h_btt_info(infop);

		id--;
	}

	free(infop);

	return offset;
}

/*
 * pmemspoil_check_field -- compares field name and moves pointer if the same
 */
static int
pmemspoil_check_field(struct pmemspoil_list *pfp, const char *fname)
{
	if (pfp->cur != NULL && strcmp(pfp->cur->name, fname) == 0) {
		return 1;
	} else {
		return 0;
	}
}

static void
pmemspoil_next_field(struct pmemspoil_list *pfp)
{
	pfp->cur = pfp->cur->next;
}

/*
 * pmemspoil_process_char -- process value as string
 */
static int
pmemspoil_process_char(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		char *str, size_t len)
{
	len = min(len, strlen(pfp->value));
	memcpy(str, pfp->value, len);

	return 0;
}

/*
 * pmemspoil_process_uint16_t -- process value as uint16
 */
static int
pmemspoil_process_uint16_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint16_t *valp, size_t size)
{
	uint16_t v;
	if (sscanf(pfp->value, "0x%" SCNx16, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu16, &v) != 1)
		return -1;
	*valp = v;

	return 0;
}

/*
 * pmemspoil_process_uint32_t -- process value as uint32
 */
static int
pmemspoil_process_uint32_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint32_t *valp, size_t size)
{
	uint32_t v;
	if (sscanf(pfp->value, "0x%" SCNx32, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu32, &v) != 1)
		return -1;
	*valp = v;

	return 0;
}

/*
 * pmemspoil_process_uint64_t -- process value as uint64
 */
static int
pmemspoil_process_uint64_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint64_t *valp, size_t size)
{
	uint64_t v;
	if (sscanf(pfp->value, "0x%" SCNx64, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu64, &v) != 1)
		return -1;
	*valp = v;

	return 0;
}

/*
 * pmemspoil_process_pool_hdr -- process pool_hdr fields
 */
int
pmemspoil_process_pool_hdr(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pool_hdr pool_hdr;
	if (pread(psp->fd, &pool_hdr, sizeof (pool_hdr), 0) !=
			sizeof (pool_hdr)) {
		return -1;
	}
	util_convert2h_pool_hdr(&pool_hdr);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&pool_hdr, signature, char);
		PROCESS_FIELD(&pool_hdr, uuid, char);
		PROCESS_FIELD(&pool_hdr, unused, char);
		PROCESS_FIELD(&pool_hdr, major, uint32_t);
		PROCESS_FIELD(&pool_hdr, compat_features, uint32_t);
		PROCESS_FIELD(&pool_hdr, incompat_features, uint32_t);
		PROCESS_FIELD(&pool_hdr, ro_compat_features, uint32_t);
		PROCESS_FIELD(&pool_hdr, crtime, uint64_t);
		PROCESS_FIELD(&pool_hdr, checksum, uint64_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		util_convert2le_pool_hdr(&pool_hdr);
		if (pwrite(psp->fd, &pool_hdr, sizeof (pool_hdr), 0) !=
				sizeof (pool_hdr)) {
			return -1;
		}
	}

	return PROCESS_RET;
}

/*
 * pmemspoil_process_btt_info_struct -- process btt_info at given offset
 */
static int
pmemspoil_process_btt_info_struct(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t offset)
{
	struct btt_info btt_info;

	if (pread(psp->fd, &btt_info, sizeof (btt_info), offset) !=
		sizeof (btt_info)) {
		return -1;
	}

	util_convert2h_btt_info(&btt_info);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&btt_info, sig, char);
		PROCESS_FIELD(&btt_info, parent_uuid, char);
		PROCESS_FIELD(&btt_info, flags, uint32_t);
		PROCESS_FIELD(&btt_info, major, uint16_t);
		PROCESS_FIELD(&btt_info, minor, uint16_t);
		PROCESS_FIELD(&btt_info, external_lbasize, uint32_t);
		PROCESS_FIELD(&btt_info, external_nlba, uint32_t);
		PROCESS_FIELD(&btt_info, internal_lbasize, uint32_t);
		PROCESS_FIELD(&btt_info, internal_nlba, uint32_t);
		PROCESS_FIELD(&btt_info, nfree, uint32_t);
		PROCESS_FIELD(&btt_info, infosize, uint32_t);
		PROCESS_FIELD(&btt_info, nextoff, uint64_t);
		PROCESS_FIELD(&btt_info, dataoff, uint64_t);
		PROCESS_FIELD(&btt_info, mapoff, uint64_t);
		PROCESS_FIELD(&btt_info, flogoff, uint64_t);
		PROCESS_FIELD(&btt_info, infooff, uint64_t);
		PROCESS_FIELD(&btt_info, unused, char);
		PROCESS_FIELD(&btt_info, checksum, uint64_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		util_convert2le_btt_info(&btt_info);

		if (pwrite(psp->fd, &btt_info, sizeof (btt_info), offset) !=
			sizeof (btt_info)) {
			return -1;
		}
	}

	return PROCESS_RET;
}

/*
 * pmemspoil_process_btt_info_backup -- process btt_info backup fields
 */
static int
pmemspoil_process_btt_info_backup(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset)
{
	struct btt_info btt_info_backup;

	if (pread(psp->fd, &btt_info_backup, sizeof (btt_info_backup),
				arena_offset) !=
		sizeof (btt_info_backup)) {
		return -1;
	}

	uint64_t backup_offset = arena_offset +
					le64toh(btt_info_backup.infooff);

	return pmemspoil_process_btt_info_struct(psp, pfp, backup_offset);
}

/*
 * pmemspoil_process_btt_info -- process btt_info fields
 */
static int
pmemspoil_process_btt_info(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset)
{
	return pmemspoil_process_btt_info_struct(psp, pfp, arena_offset);
}

/*
 * pmemspoil_process_btt_map -- process btt map fields
 */
static int
pmemspoil_process_btt_map(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset)
{
	struct btt_info btt_info;

	if (pread(psp->fd, &btt_info, sizeof (btt_info), arena_offset) !=
		sizeof (btt_info)) {
		return -1;
	}

	util_convert2h_btt_info(&btt_info);

	uint64_t mapoff = arena_offset + btt_info.mapoff;
	uint64_t mapsize = roundup(btt_info.external_nlba * BTT_MAP_ENTRY_SIZE,
							BTT_ALIGNMENT);

	uint32_t *mapp = malloc(mapsize);
	if (!mapp)
		err(1, NULL);
	int ret = 0;

	if (pread(psp->fd, mapp, mapsize, mapoff) == mapsize) {
		uint32_t v;
		if (sscanf(pfp->value, "0x%x", &v) != 1 &&
		    sscanf(pfp->value, "%u", &v) != 1) {
			ret = -1;
		} else {
			mapp[pfp->head->next->index] = v;
			if (pwrite(psp->fd, mapp, mapsize, mapoff) !=
				mapsize) {
				ret = -1;
			}
		}
	} else {
		ret = -1;
	}


	free(mapp);
	return ret;
}

/*
 * pmemspoil_process_btt_flog -- process btt_flog first or second fields
 */
static int
pmemspoil_process_btt_nflog(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset, int off)
{
	struct btt_info btt_info;
	if (pread(psp->fd, &btt_info, sizeof (btt_info), arena_offset) !=
		sizeof (btt_info)) {
		return -1;
	}

	util_convert2h_btt_info(&btt_info);

	uint64_t flogoff = arena_offset + btt_info.flogoff;
	uint64_t flogsize = btt_info.nfree *
		roundup(2 * sizeof (struct btt_flog), BTT_FLOG_PAIR_ALIGN);
	flogsize = roundup(flogsize, BTT_ALIGNMENT);

	uint8_t *flogp = malloc(flogsize);
	if (!flogp)
		err(1, NULL);

	int ret = 0;

	if (pread(psp->fd, flogp, flogsize, flogoff) != flogsize) {
		ret = -1;
		goto error;
	}

	struct btt_flog btt_flog;
	uint8_t *flog_entryp = flogp +
			pfp->head->next->index * BTT_FLOG_PAIR_ALIGN;
	if (off)
		flog_entryp += sizeof (btt_flog);

	memcpy(&btt_flog, flog_entryp, sizeof (btt_flog));

	util_convert2h_btt_flog(&btt_flog);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&btt_flog, lba, uint32_t);
		PROCESS_FIELD(&btt_flog, old_map, uint32_t);
		PROCESS_FIELD(&btt_flog, new_map, uint32_t);
		PROCESS_FIELD(&btt_flog, seq, uint32_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		util_convert2le_btt_flog(&btt_flog);

		memcpy(flog_entryp, &btt_flog, sizeof (btt_flog));

		if (pwrite(psp->fd, flogp, flogsize, flogoff)
				!= flogsize) {
			ret = -1;
			goto error;
		}

		if (pwrite(psp->fd, flogp, flogsize, flogoff) !=
			flogsize) {
			ret = -1;
			goto error;
		}
	}
	ret = PROCESS_RET;
error:
	free(flogp);

	return ret;
}

/*
 * pmemspoil_process_btt_flog -- process first btt flog entry
 */
static int
pmemspoil_process_btt_flog(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint64_t arena_offset)
{
	return pmemspoil_process_btt_nflog(psp, pfp, arena_offset, 0);
}

/*
 * pmemspoil_process_btt_flog_prime -- process second btt flog entry
 */
static int
pmemspoil_process_btt_flog_prime(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset)
{
	return pmemspoil_process_btt_nflog(psp, pfp, arena_offset, 1);
}

/*
 * pmemspoil_process_arena -- process arena fields
 */
static int
pmemspoil_process_arena(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint64_t arena_offset)
{
	if (!arena_offset)
		return -1;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS(btt_info, arena_offset, 1);
		PROCESS(btt_info_backup, arena_offset, 1);
		PROCESS(btt_map, arena_offset, 1);
		PROCESS(btt_flog, arena_offset, 1);
		PROCESS(btt_flog_prime, arena_offset, 1);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_pmemblk -- process pmemblk fields
 */
int
pmemspoil_process_pmemblk(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pmemblk pmemblk;
	if (pread(psp->fd, &pmemblk, sizeof (pmemblk), 0) !=
			sizeof (pmemblk)) {
		return -1;
	}

	pmemblk.bsize = le32toh(pmemblk.bsize);


	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&pmemblk, bsize, uint32_t);

		PROCESS(arena,
			pmemspoil_get_arena_offset(psp, PROCESS_INDEX),
			UINT64_MAX);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		pmemblk.bsize = htole32(pmemblk.bsize);

		if (pwrite(psp->fd, &pmemblk, sizeof (pmemblk), 0) !=
				sizeof (pmemblk)) {
			return -1;
		}
	}

	return PROCESS_RET;
}

/*
 * pmemspoil_process_pmemlog -- process pmemlog fields
 */
static int
pmemspoil_process_pmemlog(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pmemlog pmemlog;
	if (pread(psp->fd, &pmemlog, sizeof (pmemlog), 0) !=
			sizeof (pmemlog)) {
		return -1;
	}

	pmemlog.start_offset = le64toh(pmemlog.start_offset);
	pmemlog.end_offset = le64toh(pmemlog.end_offset);
	pmemlog.write_offset = le64toh(pmemlog.write_offset);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&pmemlog, start_offset, uint32_t);
		PROCESS_FIELD(&pmemlog, end_offset, uint32_t);
		PROCESS_FIELD(&pmemlog, write_offset, uint32_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		pmemlog.start_offset = htole64(pmemlog.start_offset);
		pmemlog.end_offset = htole64(pmemlog.end_offset);
		pmemlog.write_offset = htole64(pmemlog.write_offset);

		if (pwrite(psp->fd, &pmemlog, sizeof (pmemlog), 0) !=
				sizeof (pmemlog)) {
			return -1;
		}
	}

	return PROCESS_RET;
}
/*
 * pmemspoil_process -- process headers
 */
static int
pmemspoil_process(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS(pool_hdr, NULL, 1);
		PROCESS(pmemlog, NULL, 1);
		PROCESS(pmemblk, NULL, 1);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_func -- main function for check command
 */
int
main(int argc, char *argv[])
{
	char *appname = basename(argv[0]);
	int ret = 0;
	struct pmemspoil *psp = malloc(sizeof (struct pmemspoil));
	if (!psp)
		err(1, NULL);

	/* initialize command line arguments and context to default values */
	memcpy(psp, &pmemspoil_default, sizeof (*psp));

	/* parse command line arguments */
	ret = pmemspoil_parse_args(psp, appname, argc, argv);
	if (ret)
		goto error;

	/* set verbose level */
	out_set_vlevel(psp->verbose);

	if (psp->fname == NULL) {
		print_usage(appname);
		exit(EXIT_FAILURE);
	} else {
		if ((psp->fd = open(psp->fname, O_RDWR)) < 0)
			err(1, "%s", psp->fname);
	}

	out_set_prefix(psp->fname);

	int i;
	for (i = 0; i < psp->argc; i++) {
		pmemspoil_process(psp, &psp->args[i]);
	}

error:
	if (psp != NULL) {
		if (psp->args) {
			int i;
			for (i = 0; i < psp->argc; i++)
				pmemspoil_free_fields(&psp->args[i]);
			free(psp->args);
		}
		free(psp);
	}

	return ret;
}
