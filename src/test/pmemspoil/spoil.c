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
#include <sys/stat.h>
#include <sys/mman.h>
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

#define	PROCESS_NAME(_name, _func, _arg, _max) do {\
if (pmemspoil_check_field(_pfp, (_name))) {\
	PROCESS_STATE = PROCESS_STATE_FOUND;\
	if (_pfp->cur->index >= (_max)) {\
		PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
	} else {\
		typeof(_arg) a = _arg;\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_func(_psp, _pfp, a))\
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

#define	PROCESS_FUNC(_name, _func, _arg) do {\
	if (pmemspoil_check_field(_pfp, (_name))) {\
		PROCESS_STATE = PROCESS_STATE_FOUND;\
		if (!_pfp->str) {\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		} else {\
			if (pmemspoil_process_##_func(_psp, _pfp, (_arg)))\
				PROCESS_STATE = PROCESS_STATE_ERROR;\
		}\
		goto _process_end;\
	}\
} while (0)

#define	PROCESS_FIELD_ARRAY(_ptr, _name, _type, _max) do {\
if (pmemspoil_check_field(_pfp, STR(_name))) {\
	if (_pfp->cur->index >= (_max)) {\
		PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
	} else {\
		uint64_t ind = PROCESS_INDEX;\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_type(_psp, _pfp,\
				(_type *)&((_ptr)->_name[ind]),\
				sizeof ((_ptr)->_name)))\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		else\
			PROCESS_STATE = PROCESS_STATE_FIELD;\
	}\
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
	int is_func;
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
	void *addr;
	size_t size;
};

typedef enum chunk_type chunk_type_t;

/*
 * struct chunk_pair -- chunk header and chunk
 */
struct chunk_pair {
	struct chunk_header *hdr;
	struct chunk *chunk;
};

/*
 * struct list_pair -- list head and entry
 */
struct list_pair {
	struct list_head *head;
	struct list_entry *entry;
};

/*
 * struct checksum_args -- arguments for checksum
 */
struct checksum_args {
	void *ptr;
	size_t len;
	void *checksum;
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
	fieldp->is_func = 0;

	if (!str)
		return NULL;

	char *f = strchr(str, '.');
	if (!f)
		f = strchr(str, '=');
	if (!f) {
		f = strchr(str, '(');
		if (f && f[1] == ')')
			fieldp->is_func = 1;
	}
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

		if (fieldp->is_func)
			return f + 2;
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

/*
 * pmemspoil_next_field -- move to next field
 */
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
 * pmemspoil_process_chunk_type_t -- process chunk type
 */
static int
pmemspoil_process_chunk_type_t(struct pmemspoil *psp,
		struct pmemspoil_list *pfp,
		enum chunk_type *valp, size_t size)
{
	uint64_t types = 0;
	if (util_parse_chunk_types(pfp->value, &types))
		return -1;

	if (util_count_ones(types) != 1)
		return -1;

	*valp = (enum chunk_type)(__builtin_ffsll(types) - 1);

	return 0;
}

/*
 * pmemspoil_process_PMEMoid -- process PMEMoid value
 */
static int
pmemspoil_process_PMEMoid(struct pmemspoil *psp,
		struct pmemspoil_list *pfp,
		PMEMoid *valp, size_t size)
{
	PMEMoid v;
	if (sscanf(pfp->value, "0x%" SCNx64 ",0x%" SCNx64,
		&v.pool_uuid_lo, &v.off) != 2)
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
 * pmemspoil_process_run -- process pmemobj chunk as run
 */
static int
pmemspoil_process_run(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		struct chunk_pair cpair)
{
	struct chunk_header *chdr = cpair.hdr;
	struct chunk_run *run = (struct chunk_run *)cpair.chunk;

	if (chdr->type != CHUNK_TYPE_RUN) {
		out_err("%s -- specified chunk is not run", pfp->str);
		return -1;
	}

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(run, block_size, uint64_t);
		PROCESS_FIELD_ARRAY(run, bitmap, uint64_t, MAX_BITMAP_VALUES);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_chunk -- process pmemobj chunk structures
 */
static int
pmemspoil_process_chunk(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		struct chunk_pair cpair)
{
	struct chunk_header *chdr = cpair.hdr;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(chdr, type, chunk_type_t);
		PROCESS_FIELD(chdr, flags, uint16_t);
		PROCESS_FIELD(chdr, size_idx, uint32_t);

		PROCESS(run, cpair, 1);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_zone -- process pmemobj zone structures
 */
static int
pmemspoil_process_zone(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		struct zone *zone)
{
	struct zone_header *zhdr = &zone->header;

	PROCESS_BEGIN(psp, pfp) {
		struct chunk_pair cpair = {
			.hdr = &zone->chunk_headers[PROCESS_INDEX],
			.chunk = &zone->chunks[PROCESS_INDEX],
		};

		PROCESS_FIELD(zhdr, magic, uint32_t);
		PROCESS_FIELD(zhdr, size_idx, uint32_t);
		PROCESS_FIELD(zhdr, reserved, char);

		PROCESS(chunk, cpair, zhdr->size_idx);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_heap -- process pmemobj heap structures
 */
static int
pmemspoil_process_heap(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		struct heap_layout *hlayout)
{
	struct heap_header *hdr = &hlayout->header;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(hdr, signature, char);
		PROCESS_FIELD(hdr, major, uint64_t);
		PROCESS_FIELD(hdr, minor, uint64_t);
		PROCESS_FIELD(hdr, size, uint64_t);
		PROCESS_FIELD(hdr, chunksize, uint64_t);
		PROCESS_FIELD(hdr, chunks_per_zone, uint64_t);
		PROCESS_FIELD(hdr, reserved, char);
		PROCESS_FIELD(hdr, checksum, uint64_t);

		PROCESS(zone, &hlayout->zones[PROCESS_INDEX],
			util_heap_max_zone(psp->size));

	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_redo_log -- process redo log
 */
static int
pmemspoil_process_redo_log(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct redo_log *redo)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(redo, offset, uint64_t);
		PROCESS_FIELD(redo, value, uint64_t);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_allocator -- process lane allocator section
 */
static int
pmemspoil_process_sec_allocator(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct allocator_lane_section *sec)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS(redo_log, &sec->redo[PROCESS_INDEX], REDO_LOG_SIZE);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_entry_remove -- remove list entry
 */
static int
pmemspoil_process_entry_remove(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct list_pair lpair)
{
	struct list_head *head = lpair.head;
	struct list_entry *entry = lpair.entry;
	struct pmemobjpool *pop = psp->addr;
	struct list_entry *first = PLIST_OFF_TO_PTR(pop, head->pe_first.off);
	struct list_entry *prev = PLIST_OFF_TO_PTR(pop, entry->pe_prev.off);
	struct list_entry *next = PLIST_OFF_TO_PTR(pop, entry->pe_next.off);

	if (prev == next) {
		head->pe_first.off = 0;
	} else {
		prev->pe_next.off = entry->pe_next.off;
		next->pe_prev.off = entry->pe_prev.off;
		if (first == entry)
			head->pe_first.off = entry->pe_next.off;
	}

	return 0;
}

/*
 * pmemspoil_process_oob -- process oob header fields
 */
static int
pmemspoil_process_oob(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct list_entry *entry)
{
	struct oob_header *oob = ENTRY_TO_OOB_HDR(entry);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(oob, internal_type, uint16_t);
		PROCESS_FIELD(oob, user_type, uint16_t);
		PROCESS_FIELD(oob, size, uint64_t);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_tx_range -- process tx range fields
 */
static int
pmemspoil_process_tx_range(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct list_entry *entry)
{
	struct tx_range *range = ENTRY_TO_TX_RANGE(entry);

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(range, offset, uint64_t);
		PROCESS_FIELD(range, size, uint64_t);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_entry -- process list entry
 */
static int
pmemspoil_process_entry(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct list_pair lpair)
{
	struct list_entry *entry = lpair.entry;
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(entry, pe_next, PMEMoid);
		PROCESS_FIELD(entry, pe_prev, PMEMoid);
		PROCESS(oob, entry, 1);
		PROCESS(tx_range, entry, 1);

		PROCESS_FUNC("remove", entry_remove, lpair);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_list -- process list head
 */
static int
pmemspoil_process_list(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct list_head *head)
{
	size_t nelements = util_plist_nelements(psp->addr, head);

	PROCESS_BEGIN(psp, pfp) {
		struct list_pair lpair = {
			.head = head,
			.entry = util_plist_get_entry(psp->addr,
					head, PROCESS_INDEX),
		};

		PROCESS_FIELD(head, pe_first, PMEMoid);

		PROCESS(entry, lpair, nelements);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_tx -- process lane transaction section
 */
static int
pmemspoil_process_sec_tx(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct lane_tx_layout *sec)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(sec, state, uint64_t);
		PROCESS_NAME("undo_alloc", list, &sec->undo_alloc, 1);
		PROCESS_NAME("undo_set", list, &sec->undo_set, 1);
		PROCESS_NAME("undo_free", list, &sec->undo_free, 1);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_list -- process lane list section
 */
static int
pmemspoil_process_sec_list(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, struct lane_list_section *sec)
{
	size_t redo_size = REDO_NUM_ENTRIES;
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(sec, obj_offset, uint64_t);
		PROCESS_FIELD(sec, obj_size, uint64_t);
		PROCESS(redo_log, &sec->redo[PROCESS_INDEX], redo_size);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_lane -- process pmemobj lanes
 */
static int
pmemspoil_process_lane(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		struct lane_layout *lane)
{
	struct lane_tx_layout *sec_tx = (struct lane_tx_layout *)
		&lane->sections[LANE_SECTION_TRANSACTION];
	struct lane_list_section *sec_list = (struct lane_list_section *)
		&lane->sections[LANE_SECTION_LIST];
	struct allocator_lane_section *sec_alloc =
		(struct allocator_lane_section *)
		&lane->sections[LANE_SECTION_ALLOCATOR];

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_NAME("allocator", sec_allocator, sec_alloc, 1);
		PROCESS_NAME("tx", sec_tx, sec_tx, 1);
		PROCESS_NAME("list", sec_list, sec_list, 1);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_obj_store -- process object store structures
 */
static int
pmemspoil_process_obj_store(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, struct object_store *obj_store)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_NAME("type", list,
			&obj_store->bytype[PROCESS_INDEX].head,
			PMEMOBJ_NUM_OID_TYPES);
	} PROCESS_END
	return PROCESS_RET;
}

/*
 * pmemspoil_process_checksum_gen -- generate checksum
 */
static int
pmemspoil_process_checksum_gen(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, struct checksum_args args)
{
	util_checksum(args.ptr, args.len, args.checksum, 1);
	return 0;
}

/*
 * pmemspoil_process_pmemobj -- process pmemobj data structures
 */
static int
pmemspoil_process_pmemobj(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct stat buf;

	if (fstat(psp->fd, &buf)) {
		perror("fstat");
		return -1;
	}

	psp->size = buf.st_size;
	psp->addr = mmap(NULL, buf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED,
			psp->fd, 0);
	if (psp->addr == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	struct pmemobjpool *pop = psp->addr;
	struct heap_layout *hlayout = (void *)pop + pop->heap_offset;
	struct lane_layout *lanes = (void *)pop + pop->lanes_offset;
	struct object_store *obj_store = (void *)pop + pop->obj_store_offset;
	int ret = 0;

	PROCESS_BEGIN(psp, pfp) {
		struct checksum_args checksum_args = {
			.ptr = pop,
			.len = OBJ_DSC_P_SIZE,
			.checksum = &pop->checksum,
		};

		PROCESS_FIELD(pop, layout, char);
		PROCESS_FIELD(pop, lanes_offset, uint64_t);
		PROCESS_FIELD(pop, nlanes, uint64_t);
		PROCESS_FIELD(pop, obj_store_offset, uint64_t);
		PROCESS_FIELD(pop, obj_store_size, uint64_t);
		PROCESS_FIELD(pop, heap_offset, uint64_t);
		PROCESS_FIELD(pop, heap_size, uint64_t);
		PROCESS_FIELD(pop, unused, char);
		PROCESS_FIELD(pop, checksum, uint64_t);
		PROCESS_FIELD(pop, run_id, uint64_t);

		PROCESS_FUNC("checksum_gen", checksum_gen, checksum_args);

		PROCESS(heap, hlayout, 1);
		PROCESS(lane, &lanes[PROCESS_INDEX], pop->nlanes);
		PROCESS(obj_store, obj_store, 1);
	} PROCESS_END

	munmap(psp->addr, psp->size);
	return ret;
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
		PROCESS(pmemobj, NULL, 1);
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
