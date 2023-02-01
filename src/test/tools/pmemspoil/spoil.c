// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * spoil.c -- pmempool spoil command source file
 */
#include <features.h>
#ifndef __FreeBSD__
#define __USE_UNIX98
#endif
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
#include <assert.h>
#include <endian.h>
#include <libpmem.h>
#include "common.h"
#include "output.h"
#include "btt.h"
#include "set.h"
#include "util.h"

#define STR(x)	#x

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
	PROCESS_STATE_FUNC,
	PROCESS_STATE_ERROR_MSG,
	PROCESS_STATE_ERROR,
};

#define PROCESS_BEGIN(psp, pfp) \
enum process_state PROCESS_STATE = PROCESS_STATE_NOT_FOUND;\
struct pmemspoil *_psp = (psp);\
struct pmemspoil_list *_pfp = (pfp);\

#define PROCESS_RET ((PROCESS_STATE == PROCESS_STATE_FOUND ||\
			PROCESS_STATE == PROCESS_STATE_FIELD ||\
			PROCESS_STATE == PROCESS_STATE_FUNC) ? 0 : -1)

#define PROCESS_INDEX	(_pfp->cur->index)

#define PROCESS_END \
_process_end:\
switch (PROCESS_STATE) {\
case PROCESS_STATE_NOT_FOUND:\
	outv_err("unknown field '%s'\n", _pfp->cur->name);\
	break;\
case PROCESS_STATE_FIELD:\
	outv(2, "spoil: %s\n", _pfp->str);\
	break;\
case PROCESS_STATE_FUNC:\
	outv(2, "spoil: %s\n", _pfp->str);\
	break;\
case PROCESS_STATE_ERROR_MSG:\
	outv_err("processing '%s'\n", _pfp->str);\
	PROCESS_STATE = PROCESS_STATE_ERROR;\
	break;\
default:\
	break;\
}

/* _max - size of _arg if it is array (if not it must be 1) */
#define PROCESS(_name, _arg, _max, _type) do {\
if (pmemspoil_check_field(_pfp, STR(_name))) {\
	PROCESS_STATE = PROCESS_STATE_FOUND;\
	if (_pfp->cur->index >= (_max)) {\
		PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
	} else {\
		_type a = _arg;\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_name(_psp, _pfp, a))\
			PROCESS_STATE = PROCESS_STATE_ERROR;\
	}\
	goto _process_end;\
}\
} while (0)

#define PROCESS_FIELD(_ptr, _name, _type) do {\
	if (pmemspoil_check_field(_pfp, STR(_name))) {\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_type(_psp, _pfp,\
				(_type *)&((_ptr)->_name),\
				sizeof((_ptr)->_name), 0))\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		else\
			PROCESS_STATE = PROCESS_STATE_FIELD;\
		goto _process_end;\
	}\
} while (0)

#define PROCESS_FIELD_LE(_ptr, _name, _type) do {\
	if (pmemspoil_check_field(_pfp, STR(_name))) {\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_type(_psp, _pfp,\
				(_type *)&((_ptr)->_name),\
				sizeof((_ptr)->_name), 1))\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		else\
			PROCESS_STATE = PROCESS_STATE_FIELD;\
		goto _process_end;\
	}\
} while (0)

#define PROCESS_FUNC(_name, _func, _arg) do {\
	if (pmemspoil_check_field(_pfp, (_name))) {\
		PROCESS_STATE = PROCESS_STATE_FOUND;\
		if (!_pfp->str) {\
			PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
		} else {\
			if (pmemspoil_process_##_func(_psp, _pfp, (_arg)))\
				PROCESS_STATE = PROCESS_STATE_ERROR;\
			else\
				PROCESS_STATE = PROCESS_STATE_FUNC;\
		}\
		goto _process_end;\
	}\
} while (0)

#define PROCESS_FIELD_ARRAY(_ptr, _name, _type, _max) do {\
if (pmemspoil_check_field(_pfp, STR(_name))) {\
	if (_pfp->cur->index >= (_max)) {\
		PROCESS_STATE = PROCESS_STATE_ERROR_MSG;\
	} else {\
		uint64_t ind = PROCESS_INDEX;\
		pmemspoil_next_field(_pfp);\
		if (pmemspoil_process_##_type(_psp, _pfp,\
				(_type *)&((_ptr)->_name[ind]),\
				sizeof((_ptr)->_name), 0))\
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
	uint32_t index;
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
	struct pool_set_file *pfile;
	struct pmemspoil_list *args;
	unsigned argc;
	void *addr;
	size_t size;
	unsigned replica;
	uint64_t arena_offset;
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
	size_t skip_off;
};

/*
 * pmemspoil_default -- default context and args
 */
static const struct pmemspoil pmemspoil_default = {
	.verbose	= 1,
	.fname		= NULL,
	.args		= NULL,
	.argc		= 0,
	.replica	= 0,
};

/*
 * help_str -- string for help message
 */
static const char * const  help_str =
"%s common options:\n"
"  -v, --verbose        Increase verbose level\n"
"  -?, --help           Display this help and exit\n"
"  -r, --replica <num>  Replica index\n"
"\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"verbose",	no_argument,		NULL,	'v'},
	{"help",	no_argument,		NULL,	'?'},
	{"replica",	required_argument,	NULL,	'r'},
	{NULL,		0,			NULL,	 0 },
};

/*
 * pmemspoil_persist -- flush data to persistence
 */
static void
pmemspoil_persist(void *addr, size_t size)
{
	if (pmem_is_pmem(addr, size))
		pmem_persist(addr, size);
	else
		pmem_msync(addr, size);
}

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
 * pmemspoil_help -- print help message for spoil command
 */
static void
pmemspoil_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmemspoil_read -- read data from pool
 */
static int
pmemspoil_read(struct pmemspoil *psp, void *buff, size_t nbytes, uint64_t off)
{
	return pool_set_file_read(psp->pfile, buff, nbytes, off);
}

/*
 * pmemspoil_write -- write data to pool
 */
static int
pmemspoil_write(struct pmemspoil *psp, void *buff, size_t nbytes, uint64_t off)
{
	return pool_set_file_write(psp->pfile, buff, nbytes, off);
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
		if (str[0] == 'f' && str[1] == ':') {
			f = str + 2;
			fieldp->is_func = 1;
		}

	}
	fieldp->index = 0;
	fieldp->name = NULL;
	if (f) {
		if (fieldp->is_func)
			str = f;
		else
			*f = '\0';
		size_t len = 0;
		ssize_t ret;
		char *secstr = malloc(strlen(str) + 1);
		uint32_t secind;
		/* search for pattern: <field_name>(<index>) */
		if (secstr == NULL)
			err(1, NULL);
		if ((ret = sscanf(str, "%[^(](%d)", secstr, &secind) == 2)) {
			len = strlen(secstr);
			str[len] = '\0';
			fieldp->index = secind;
		}

		fieldp->name = str;
		free(secstr);
		if (fieldp->is_func)
			return str + strlen(str);
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
		struct field *fp = malloc(sizeof(struct field));
		if (!fp) {
			pmemspoil_free_fields(listp);
			err(1, NULL);
		}
		memcpy(fp, &f, sizeof(*fp));
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
	int t;
	while ((opt = getopt_long(argc, argv, "v?r:",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'v':
			psp->verbose = 2;
			break;
		case '?':
			pmemspoil_help(appname);
			exit(EXIT_SUCCESS);
		case 'r':
			t = atoi(optarg);
			if (t < 0) {
				print_usage(appname);
				exit(EXIT_FAILURE);
			}
			psp->replica = (unsigned)t;
			break;
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		int ind = optind;
		psp->fname = argv[ind];
		ind++;

		assert(argc >= ind);
		psp->argc = (unsigned)(argc - ind);
		psp->args = calloc(psp->argc, sizeof(struct pmemspoil_list));
		if (!psp->args)
			err(1, NULL);
		unsigned i;
		for (i = 0; i < psp->argc; i++) {
			char *str = argv[ind];
			if (pmemspoil_parse_fields(str, &psp->args[i])) {
				outv_err("ivalid argument");
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
pmemspoil_get_arena_offset(struct pmemspoil *psp, uint32_t id,
		uint64_t start_offset)
{
	struct btt_info *infop = calloc(sizeof(struct btt_info), 1);
	if (!infop)
		err(1, NULL);

	infop->nextoff = start_offset;

	uint64_t offset = 0;
	ssize_t ret = 0;
	id++;
	while (id > 0) {
		if (infop->nextoff == 0) {
			free(infop);
			return 0;
		}
		offset = offset + infop->nextoff;
		if ((ret = pmemspoil_read(psp, infop,
				sizeof(*infop), offset))) {
			free(infop);
			return 0;
		}

		btt_info_convert2h(infop);

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
		char *str, size_t len, int le)
{
	len = min(len, strlen(pfp->value));
	memcpy(str, pfp->value, len);

	pmemspoil_persist(str, len);

	return 0;
}

/*
 * pmemspoil_process_uint8_t -- process value as uint8
 */
static int
pmemspoil_process_uint8_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint8_t *valp, size_t size, int le)
{
	uint8_t v;
	if (sscanf(pfp->value, "0x%" SCNx8, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu8, &v) != 1)
		return -1;
	*valp = v;

	pmemspoil_persist(valp, sizeof(*valp));

	return 0;
}

/*
 * pmemspoil_process_uint16_t -- process value as uint16
 */
static int
pmemspoil_process_uint16_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint16_t *valp, size_t size, int le)
{
	uint16_t v;
	if (sscanf(pfp->value, "0x%" SCNx16, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu16, &v) != 1)
		return -1;
	if (le)
		*valp = htole16(v);
	else
		*valp = v;

	pmemspoil_persist(valp, sizeof(*valp));

	return 0;
}

/*
 * pmemspoil_process_uint32_t -- process value as uint32
 */
static int
pmemspoil_process_uint32_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint32_t *valp, size_t size, int le)
{
	uint32_t v;
	if (sscanf(pfp->value, "0x%" SCNx32, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu32, &v) != 1)
		return -1;
	if (le)
		*valp = htole32(v);
	else
		*valp = v;

	pmemspoil_persist(valp, sizeof(*valp));

	return 0;
}

/*
 * pmemspoil_process_uint64_t -- process value as uint64
 */
static int
pmemspoil_process_uint64_t(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint64_t *valp, size_t size, int le)
{
	uint64_t v;
	if (sscanf(pfp->value, "0x%" SCNx64, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu64, &v) != 1)
		return -1;
	if (le)
		*valp = htole64(v);
	else
		*valp = v;

	pmemspoil_persist(valp, sizeof(*valp));

	return 0;
}

/*
 * pmemspoil_process_chunk_type_t -- process chunk type
 */
static int
pmemspoil_process_chunk_type_t(struct pmemspoil *psp,
		struct pmemspoil_list *pfp,
		enum chunk_type *valp, size_t size, int le)
{
	uint64_t types = 0;
	if (util_parse_chunk_types(pfp->value, &types))
		return -1;

	if (util_popcount64(types) != 1)
		return -1;

	/* ignore 'le' */
	*valp = (enum chunk_type)util_lssb_index64(types);

	return 0;
}

/*
 * pmemspoil_process_checksum_gen -- generate checksum
 */
static int
pmemspoil_process_checksum_gen(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, struct checksum_args args)
{
	util_checksum(args.ptr, args.len, (uint64_t *)args.checksum,
		1, args.skip_off);
	return 0;
}

/*
 * pmemspoil_process_shutdown_state -- process shutdown_state fields
 */
static int
pmemspoil_process_shutdown_state(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, void *arg)
{
	struct shutdown_state *sds = arg;
	PROCESS_BEGIN(psp, pfp) {
		struct checksum_args checksum_args = {
			.ptr = sds,
			.len = sizeof(*sds),
			.checksum = &sds->checksum,
			.skip_off = 0,
		};

		PROCESS_FIELD_LE(sds, usc, uint64_t);
		PROCESS_FIELD_LE(sds, uuid, uint64_t);
		PROCESS_FIELD_LE(sds, dirty, uint64_t);
		PROCESS_FIELD(sds, reserved, char);
		PROCESS_FIELD_LE(sds, checksum, uint64_t);

		PROCESS_FUNC("checksum_gen", checksum_gen, checksum_args);
	} PROCESS_END;

	return PROCESS_RET;
}

/*
 * pmemspoil_process_features -- process features fields
 */
static int
pmemspoil_process_features(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, void *arg)
{
	features_t *features = arg;
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD_LE(features, compat, uint32_t);
		PROCESS_FIELD_LE(features, incompat, uint32_t);
		PROCESS_FIELD_LE(features, ro_compat, uint32_t);
	} PROCESS_END;

	return PROCESS_RET;
}

/*
 * pmemspoil_process_pool_hdr -- process pool_hdr fields
 */
static int
pmemspoil_process_pool_hdr(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pool_hdr pool_hdr;
	if (pmemspoil_read(psp, &pool_hdr, sizeof(pool_hdr), 0))
		return -1;

	PROCESS_BEGIN(psp, pfp) {
		struct checksum_args checksum_args = {
			.ptr = &pool_hdr,
			.len = sizeof(pool_hdr),
			.checksum = &pool_hdr.checksum,
			.skip_off = POOL_HDR_CSUM_END_OFF(&pool_hdr),
		};

		PROCESS_FIELD(&pool_hdr, signature, char);
		PROCESS_FIELD(&pool_hdr, poolset_uuid, char);
		PROCESS_FIELD(&pool_hdr, uuid, char);
		PROCESS_FIELD(&pool_hdr, prev_part_uuid, char);
		PROCESS_FIELD(&pool_hdr, next_part_uuid, char);
		PROCESS_FIELD(&pool_hdr, prev_repl_uuid, char);
		PROCESS_FIELD(&pool_hdr, next_repl_uuid, char);
		PROCESS_FIELD(&pool_hdr, unused, char);
		PROCESS_FIELD(&pool_hdr, unused2, char);
		PROCESS_FIELD_LE(&pool_hdr, major, uint32_t);
		PROCESS(features, &pool_hdr.features, 1, features_t *);
		PROCESS_FIELD_LE(&pool_hdr, crtime, uint64_t);
		PROCESS_FIELD(&pool_hdr, arch_flags, char); /* XXX */
		PROCESS(shutdown_state, &pool_hdr.sds, 1,
			struct shutdown_state *);
		PROCESS_FIELD_LE(&pool_hdr, checksum, uint64_t);

		PROCESS_FUNC("checksum_gen", checksum_gen, checksum_args);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD ||
	    PROCESS_STATE == PROCESS_STATE_FUNC ||
	    PROCESS_STATE == PROCESS_STATE_FOUND) {
		if (pmemspoil_write(psp, &pool_hdr, sizeof(pool_hdr), 0))
			return -1;
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

	if (pmemspoil_read(psp, &btt_info, sizeof(btt_info), offset))
		return -1;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(&btt_info, sig, char);
		PROCESS_FIELD(&btt_info, uuid, char);
		PROCESS_FIELD(&btt_info, parent_uuid, char);
		PROCESS_FIELD_LE(&btt_info, flags, uint32_t);
		PROCESS_FIELD_LE(&btt_info, major, uint16_t);
		PROCESS_FIELD_LE(&btt_info, minor, uint16_t);
		PROCESS_FIELD_LE(&btt_info, external_lbasize, uint32_t);
		PROCESS_FIELD_LE(&btt_info, external_nlba, uint32_t);
		PROCESS_FIELD_LE(&btt_info, internal_lbasize, uint32_t);
		PROCESS_FIELD_LE(&btt_info, internal_nlba, uint32_t);
		PROCESS_FIELD_LE(&btt_info, nfree, uint32_t);
		PROCESS_FIELD_LE(&btt_info, infosize, uint32_t);
		PROCESS_FIELD_LE(&btt_info, nextoff, uint64_t);
		PROCESS_FIELD_LE(&btt_info, dataoff, uint64_t);
		PROCESS_FIELD_LE(&btt_info, mapoff, uint64_t);
		PROCESS_FIELD_LE(&btt_info, flogoff, uint64_t);
		PROCESS_FIELD_LE(&btt_info, infooff, uint64_t);
		PROCESS_FIELD(&btt_info, unused, char);
		PROCESS_FIELD_LE(&btt_info, checksum, uint64_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		if (pmemspoil_write(psp, &btt_info, sizeof(btt_info), offset))
			return -1;
	}

	return PROCESS_RET;
}

/*
 * pmemspoil_process_btt_info_backup -- process btt_info backup fields
 */
static int
pmemspoil_process_btt_info_backup(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint32_t index)
{
	struct btt_info btt_info_backup;

	if (pmemspoil_read(psp, &btt_info_backup, sizeof(btt_info_backup),
				psp->arena_offset))
		return -1;

	uint64_t backup_offset = psp->arena_offset +
			le64toh(btt_info_backup.infooff);

	return pmemspoil_process_btt_info_struct(psp, pfp, backup_offset);
}

/*
 * pmemspoil_process_btt_info -- process btt_info fields
 */
static int
pmemspoil_process_btt_info(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint32_t index)
{
	return pmemspoil_process_btt_info_struct(psp, pfp, psp->arena_offset);
}

/*
 * pmemspoil_process_btt_map -- process btt map fields
 */
static int
pmemspoil_process_btt_map(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint32_t index)
{
	struct btt_info btt_info;

	if (pmemspoil_read(psp, &btt_info, sizeof(btt_info),
			psp->arena_offset))
		return -1;

	btt_info_convert2h(&btt_info);

	uint64_t mapoff = psp->arena_offset + btt_info.mapoff;
	uint64_t mapsize = roundup(btt_info.external_nlba * BTT_MAP_ENTRY_SIZE,
							BTT_ALIGNMENT);

	uint32_t *mapp = malloc(mapsize);
	if (!mapp)
		err(1, NULL);
	int ret = 0;

	if (pmemspoil_read(psp, mapp, mapsize, mapoff)) {
		ret = -1;
	} else {
		uint32_t v;
		if (sscanf(pfp->value, "0x%x", &v) != 1 &&
		    sscanf(pfp->value, "%u", &v) != 1) {
			ret = -1;
		} else {
			mapp[index] = v;
			if (pmemspoil_write(psp, mapp, mapsize, mapoff))
				ret = -1;
		}
	}

	free(mapp);
	return ret;
}

/*
 * pmemspoil_process_btt_flog -- process btt_flog first or second fields
 */
static int
pmemspoil_process_btt_nflog(struct pmemspoil *psp,
	struct pmemspoil_list *pfp, uint64_t arena_offset, int off,
	uint32_t index)
{
	struct btt_info btt_info;
	if (pmemspoil_read(psp, &btt_info, sizeof(btt_info), arena_offset))
		return -1;

	btt_info_convert2h(&btt_info);

	uint64_t flogoff = arena_offset + btt_info.flogoff;
	uint64_t flogsize = btt_info.nfree *
		roundup(2 * sizeof(struct btt_flog), BTT_FLOG_PAIR_ALIGN);
	flogsize = roundup(flogsize, BTT_ALIGNMENT);

	uint8_t *flogp = malloc(flogsize);
	if (!flogp)
		err(1, NULL);

	int ret = 0;

	if (pmemspoil_read(psp, flogp, flogsize, flogoff)) {
		ret = -1;
		goto error;
	}

	struct btt_flog *flog_entryp = (struct btt_flog *)(flogp +
			index * BTT_FLOG_PAIR_ALIGN);
	if (off)
		flog_entryp++;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD_LE(flog_entryp, lba, uint32_t);
		PROCESS_FIELD_LE(flog_entryp, old_map, uint32_t);
		PROCESS_FIELD_LE(flog_entryp, new_map, uint32_t);
		PROCESS_FIELD_LE(flog_entryp, seq, uint32_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		if (pmemspoil_write(psp, flogp, flogsize, flogoff)) {
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
		uint32_t index)
{
	return pmemspoil_process_btt_nflog(psp, pfp,
		psp->arena_offset, 0, index);
}

/*
 * pmemspoil_process_btt_flog_prime -- process second btt flog entry
 */
static int
pmemspoil_process_btt_flog_prime(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, uint32_t index)
{
	return pmemspoil_process_btt_nflog(psp, pfp,
		psp->arena_offset, 1, index);
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

	struct btt_info btt_info;
	if (pmemspoil_read(psp, &btt_info, sizeof(btt_info), arena_offset))
		return -1;

	btt_info_convert2h(&btt_info);
	psp->arena_offset = arena_offset;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS(btt_info, PROCESS_INDEX, 1, uint32_t);
		PROCESS(btt_info_backup, PROCESS_INDEX, 1, uint32_t);
		PROCESS(btt_map, PROCESS_INDEX, btt_info.external_nlba,
			uint32_t);
		PROCESS(btt_flog, PROCESS_INDEX, btt_info.nfree, uint32_t);
		PROCESS(btt_flog_prime, PROCESS_INDEX, btt_info.nfree,
			uint32_t);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_pmemblk -- process pmemblk fields
 */
static int
pmemspoil_process_pmemblk(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pmemblk pmemblk;
	if (pmemspoil_read(psp, &pmemblk, sizeof(pmemblk), 0))
		return -1;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD_LE(&pmemblk, bsize, uint32_t);

		PROCESS(arena,
			pmemspoil_get_arena_offset(psp, PROCESS_INDEX,
				ALIGN_UP(sizeof(struct pmemblk),
					BLK_FORMAT_DATA_ALIGN)),
			UINT32_MAX, uint64_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		if (pmemspoil_write(psp, &pmemblk, sizeof(pmemblk), 0))
			return -1;
	}

	return PROCESS_RET;
}

/*
 * pmemspoil_process_bttdevice -- process btt device fields
 */
static int
pmemspoil_process_bttdevice(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	PROCESS_BEGIN(psp, pfp) {
		PROCESS(arena,
			pmemspoil_get_arena_offset(psp, PROCESS_INDEX,
					ALIGN_UP(sizeof(struct pool_hdr),
					BTT_ALIGNMENT)),
			UINT32_MAX, uint64_t);
	} PROCESS_END
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
	if (pmemspoil_read(psp, &pmemlog, sizeof(pmemlog), 0))
		return -1;

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD_LE(&pmemlog, start_offset, uint64_t);
		PROCESS_FIELD_LE(&pmemlog, end_offset, uint64_t);
		PROCESS_FIELD_LE(&pmemlog, write_offset, uint64_t);
	} PROCESS_END

	if (PROCESS_STATE == PROCESS_STATE_FIELD) {
		if (pmemspoil_write(psp, &pmemlog, sizeof(pmemlog), 0))
			return -1;
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
		outv_err("%s -- specified chunk is not run", pfp->str);
		return -1;
	}

	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD(run, hdr.block_size, uint64_t);
		PROCESS_FIELD_ARRAY(run, content, uint8_t, RUN_CONTENT_SIZE);
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

		PROCESS(run, cpair, 1, struct chunk_pair);
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

		PROCESS(chunk, cpair, zhdr->size_idx, struct chunk_pair);
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
		PROCESS_FIELD(hdr, unused, uint64_t);
		PROCESS_FIELD(hdr, chunksize, uint64_t);
		PROCESS_FIELD(hdr, chunks_per_zone, uint64_t);
		PROCESS_FIELD(hdr, reserved, char);
		PROCESS_FIELD(hdr, checksum, uint64_t);

		PROCESS(zone, ZID_TO_ZONE(hlayout, PROCESS_INDEX),
			util_heap_max_zone(psp->size), struct zone *);

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
	PROCESS_BEGIN(psp, pfp) {
		PROCESS_FIELD_ARRAY(lane, undo.data,
			uint8_t, LANE_UNDO_SIZE);
		PROCESS_FIELD_ARRAY(lane, internal.data,
			uint8_t, LANE_REDO_INTERNAL_SIZE);
		PROCESS_FIELD_ARRAY(lane, external.data,
			uint8_t, LANE_REDO_EXTERNAL_SIZE);
	} PROCESS_END

	return PROCESS_RET;
}

/*
 * pmemspoil_process_pmemobj -- process pmemobj data structures
 */
static int
pmemspoil_process_pmemobj(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, void *arg)
{
	struct pmemobjpool *pop = psp->addr;
	struct heap_layout *hlayout = (void *)((char *)pop + pop->heap_offset);
	struct lane_layout *lanes = (void *)((char *)pop + pop->lanes_offset);

	PROCESS_BEGIN(psp, pfp) {
		struct checksum_args checksum_args = {
			.ptr = pop,
			.len = OBJ_DSC_P_SIZE,
			.checksum = &pop->checksum,
			.skip_off = 0,
		};

		PROCESS_FIELD(pop, layout, char);
		PROCESS_FIELD(pop, lanes_offset, uint64_t);
		PROCESS_FIELD(pop, nlanes, uint64_t);
		PROCESS_FIELD(pop, heap_offset, uint64_t);
		PROCESS_FIELD(pop, unused3, uint64_t);
		PROCESS_FIELD(pop, unused, char);
		PROCESS_FIELD(pop, checksum, uint64_t);
		PROCESS_FIELD(pop, run_id, uint64_t);

		PROCESS_FUNC("checksum_gen", checksum_gen, checksum_args);

		PROCESS(heap, hlayout, 1, struct heap_layout *);
		PROCESS(lane, &lanes[PROCESS_INDEX], pop->nlanes,
			struct lane_layout *);
	} PROCESS_END

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
		PROCESS(pool_hdr, NULL, 1, void *);
		PROCESS(pmemlog, NULL, 1, void *);
		PROCESS(pmemblk, NULL, 1, void *);
		PROCESS(pmemobj, NULL, 1, void *);
		PROCESS(bttdevice, NULL, 1, void *);
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
	util_init();
	int ret = 0;
	struct pmemspoil *psp = malloc(sizeof(struct pmemspoil));
	if (!psp)
		err(1, NULL);

	/* initialize command line arguments and context to default values */
	memcpy(psp, &pmemspoil_default, sizeof(*psp));

	/* parse command line arguments */
	ret = pmemspoil_parse_args(psp, appname, argc, argv);
	if (ret)
		goto error;

	/* set verbose level */
	out_set_vlevel(psp->verbose);

	if (psp->fname == NULL) {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	psp->pfile = pool_set_file_open(psp->fname, 0, 1);
	if (!psp->pfile)
		err(1, "%s", psp->fname);

	if (pool_set_file_set_replica(psp->pfile, psp->replica)) {
		outv_err("invalid replica argument max is %u\n",
				psp->pfile->poolset ?
				psp->pfile->poolset->nreplicas :
				0);
		return 1;
	}

	psp->addr = pool_set_file_map(psp->pfile, 0);
	psp->size = psp->pfile->size;

	out_set_prefix(psp->fname);

	for (unsigned i = 0; i < psp->argc; i++) {
		ret = pmemspoil_process(psp, &psp->args[i]);
		if (ret)
			goto error;
	}

error:
	if (psp->args) {
		for (unsigned i = 0; i < psp->argc; i++)
			pmemspoil_free_fields(&psp->args[i]);
		free(psp->args);
	}

	pool_set_file_close(psp->pfile);

	free(psp);
	return ret;
}
