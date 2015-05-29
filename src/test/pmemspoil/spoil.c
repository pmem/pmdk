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
 * Useful macros for pmemspoil_process_* set of functions.
 * These macros take the struct and field name as an argument
 * and transforms it to address, size and string.
 * Example:
 *	struct my_struct {
 *		uint16_t field;
 *	} my;
 *	PROCESS_UINT16(psp, pfp, my.field);
 */
#define	PROCESS_UINT16(psp, pfp, name)	\
pmemspoil_process_uint16((psp), (pfp), (uint16_t *)&name, STR(name))

#define	PROCESS_UINT32(psp, pfp, name)	\
pmemspoil_process_uint32((psp), (pfp), (uint32_t *)&name, STR(name))

#define	PROCESS_UINT64(psp, pfp, name)	\
pmemspoil_process_uint64((psp), (pfp), (uint64_t *)&name, STR(name))

#define	PROCESS_STRING(psp, pfp, name)	\
pmemspoil_process_string((psp), (pfp), (char *)&name, sizeof (name), STR(name))


/*
 * struct field -- single field with name and id
 */
struct field {
	struct field *next;
	struct field *prev;
	char *name;
	uint32_t index;
};

/*
 * struct pmemspoil_list -- all fields and value
 */
struct pmemspoil_list {
	struct field *head;
	struct field *tail;
	struct field *cur;
	char *value;
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

		return f+1;
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
pmemspoil_check_field(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		const char *fname)
{
	if (pfp->cur != NULL && strcmp(pfp->cur->name, fname) == 0) {
		pfp->cur = pfp->cur->next;
		return 1;
	} else {
		return 0;
	}
}

/*
 * pmemspoil_process_string -- process value as string
 */
static int
pmemspoil_process_string(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		char *str, size_t len, char *name)
{
	len = min(len, strlen(pfp->value));
	memcpy(str, pfp->value, len);
	outv(2, "spoil: %s = %s\n", name, pfp->value);

	return 0;
}

/*
 * pmemspoil_process_string -- process value as uint16
 */
static int
pmemspoil_process_uint16(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint16_t *valp, char *name)
{
	uint16_t v;
	if (sscanf(pfp->value, "0x%" SCNx16, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu16, &v) != 1)
		return -1;
	*valp = v;

	outv(2, "spoil: %s = %s\n", name, pfp->value);

	return 0;
}

/*
 * pmemspoil_process_string -- process value as uint32
 */
static int
pmemspoil_process_uint32(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint32_t *valp, char *name)
{
	uint32_t v;
	if (sscanf(pfp->value, "0x%" SCNx32, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu32, &v) != 1)
		return -1;
	*valp = v;

	outv(2, "spoil: %s = %s\n", name, pfp->value);

	return 0;
}

/*
 * pmemspoil_process_string -- process value as uint64
 */
static int
pmemspoil_process_uint64(struct pmemspoil *psp, struct pmemspoil_list *pfp,
		uint64_t *valp, char *name)
{
	uint64_t v;
	if (sscanf(pfp->value, "0x%" SCNx64, &v) != 1 &&
	    sscanf(pfp->value, "%" SCNu64, &v) != 1)
		return -1;
	*valp = v;

	outv(2, "spoil: %s = %s\n", name, pfp->value);

	return 0;
}

/*
 * pmemspoil_process_pool_hdr -- process pool_hdr fields
 */
int
pmemspoil_process_pool_hdr(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	struct pool_hdr pool_hdr;
	if (pread(psp->fd, &pool_hdr, sizeof (pool_hdr), 0) !=
			sizeof (pool_hdr)) {
		return -1;
	}
	util_convert2h_pool_hdr(&pool_hdr);

	int ret = 0;
	if (pmemspoil_check_field(psp, pfp, "signature")) {
		ret = PROCESS_STRING(psp, pfp, pool_hdr.signature);
	} else if (pmemspoil_check_field(psp, pfp, "uuid")) {
		ret = PROCESS_STRING(psp, pfp, pool_hdr.uuid);
	} else if (pmemspoil_check_field(psp, pfp, "unused")) {
		ret = PROCESS_STRING(psp, pfp, pool_hdr.unused);
	} else if (pmemspoil_check_field(psp, pfp, "major")) {
		ret = PROCESS_UINT32(psp, pfp, pool_hdr.major);
	} else if (pmemspoil_check_field(psp, pfp, "compat_features")) {
		ret = PROCESS_UINT32(psp, pfp, pool_hdr.compat_features);
	} else if (pmemspoil_check_field(psp, pfp, "incompat_features")) {
		ret = PROCESS_UINT32(psp, pfp, pool_hdr.incompat_features);
	} else if (pmemspoil_check_field(psp, pfp, "ro_compat_features")) {
		ret = PROCESS_UINT32(psp, pfp, pool_hdr.ro_compat_features);
	} else if (pmemspoil_check_field(psp, pfp, "crtime")) {
		ret = PROCESS_UINT64(psp, pfp, pool_hdr.crtime);
	} else if (pmemspoil_check_field(psp, pfp, "checksum")) {
		ret = PROCESS_UINT64(psp, pfp, pool_hdr.checksum);
	} else {
		out_err("unknown field '%s'\n", pfp->cur->name);
		return -1;
	}

	if (ret) {
		out_err("parsing value '%s'\n", pfp->value);
		return -1;
	}

	util_convert2le_pool_hdr(&pool_hdr);
	if (pwrite(psp->fd, &pool_hdr, sizeof (pool_hdr), 0) !=
			sizeof (pool_hdr)) {
		return -1;
	}

	return 0;
}

/*
 * pmemspoil_process_pmemblk -- process pmemblk fields
 */
int
pmemspoil_process_pmemblk(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	struct pmemblk pmemblk;
	if (pread(psp->fd, &pmemblk, sizeof (pmemblk), 0) !=
			sizeof (pmemblk)) {
		return -1;
	}

	pmemblk.bsize = le32toh(pmemblk.bsize);

	int ret = 0;
	if (pmemspoil_check_field(psp, pfp, "bsize")) {
		ret = PROCESS_UINT32(psp, pfp, pmemblk.bsize);
	} else {
		out_err("unknown field '%s'\n", pfp->cur->name);
		return -1;
	}

	if (ret) {
		out_err("parsing value '%s'\n", pfp->value);
		return -1;
	}

	pmemblk.bsize = htole32(pmemblk.bsize);

	if (pwrite(psp->fd, &pmemblk, sizeof (pmemblk), 0) !=
			sizeof (pmemblk)) {
		return -1;
	}

	return 0;
}

/*
 * pmemspoil_process_pmemlog -- process pmemlog fields
 */
static int
pmemspoil_process_pmemlog(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	struct pmemlog pmemlog;
	if (pread(psp->fd, &pmemlog, sizeof (pmemlog), 0) !=
			sizeof (pmemlog)) {
		return -1;
	}

	pmemlog.start_offset = le64toh(pmemlog.start_offset);
	pmemlog.end_offset = le64toh(pmemlog.end_offset);
	pmemlog.write_offset = le64toh(pmemlog.write_offset);

	int ret;
	if (pmemspoil_check_field(psp, pfp, "start_offset")) {
		ret = PROCESS_UINT32(psp, pfp, pmemlog.start_offset);
	} else if (pmemspoil_check_field(psp, pfp, "end_offset")) {
		ret = PROCESS_UINT32(psp, pfp, pmemlog.end_offset);
	} else if (pmemspoil_check_field(psp, pfp, "write_offset")) {
		ret = PROCESS_UINT32(psp, pfp, pmemlog.write_offset);
	} else {
		out_err("unknown field '%s'\n", pfp->cur->name);
		return -1;
	}

	if (ret) {
		out_err("parsing value '%s'\n", pfp->value);
		return -1;
	}

	pmemlog.start_offset = htole64(pmemlog.start_offset);
	pmemlog.end_offset = htole64(pmemlog.end_offset);
	pmemlog.write_offset = htole64(pmemlog.write_offset);

	if (pwrite(psp->fd, &pmemlog, sizeof (pmemlog), 0) !=
			sizeof (pmemlog)) {
		return -1;
	}

	return 0;
}

/*
 * pmemspoil_process_btt_info_backup -- process btt_info backup fields
 */
static int
pmemspoil_process_btt_info_backup(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	uint64_t arena_offset =
		pmemspoil_get_arena_offset(psp, pfp->head->index);

	if (!arena_offset)
		return -1;

	struct btt_info btt_info_backup;

	if (pread(psp->fd, &btt_info_backup, sizeof (btt_info_backup),
				arena_offset) !=
		sizeof (btt_info_backup)) {
		return -1;
	}

	uint64_t backup_offset = arena_offset +
					le64toh(btt_info_backup.infooff);

	if (pread(psp->fd, &btt_info_backup, sizeof (btt_info_backup),
				backup_offset) != sizeof (btt_info_backup)) {
		return -1;
	}

	util_convert2h_btt_info(&btt_info_backup);

	int ret = 0;
	if (pmemspoil_check_field(psp, pfp, "sig")) {
		ret = PROCESS_STRING(psp, pfp, btt_info_backup.sig);
	} else if (pmemspoil_check_field(psp, pfp, "parent_uuid")) {
		ret = PROCESS_STRING(psp, pfp, btt_info_backup.parent_uuid);
	} else if (pmemspoil_check_field(psp, pfp, "flags")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info_backup.flags);
	} else if (pmemspoil_check_field(psp, pfp, "major")) {
		ret = PROCESS_UINT16(psp, pfp, btt_info_backup.major);
	} else if (pmemspoil_check_field(psp, pfp, "minor")) {
		ret = PROCESS_UINT16(psp, pfp, btt_info_backup.minor);
	} else if (pmemspoil_check_field(psp, pfp, "external_lbasize")) {
		ret = PROCESS_UINT32(psp, pfp,
				btt_info_backup.external_lbasize);
	} else if (pmemspoil_check_field(psp, pfp, "external_nlba")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info_backup.external_nlba);
	} else if (pmemspoil_check_field(psp, pfp, "internal_lbasize")) {
		ret = PROCESS_UINT32(psp, pfp,
			btt_info_backup.internal_lbasize);
	} else if (pmemspoil_check_field(psp, pfp, "internal_nlba")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info_backup.internal_nlba);
	} else if (pmemspoil_check_field(psp, pfp, "nfree")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info_backup.nfree);
	} else if (pmemspoil_check_field(psp, pfp, "infosize")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info_backup.infosize);
	} else if (pmemspoil_check_field(psp, pfp, "nextoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.nextoff);
	} else if (pmemspoil_check_field(psp, pfp, "dataoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.dataoff);
	} else if (pmemspoil_check_field(psp, pfp, "mapoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.mapoff);
	} else if (pmemspoil_check_field(psp, pfp, "flogoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.flogoff);
	} else if (pmemspoil_check_field(psp, pfp, "infooff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.infooff);
	} else if (pmemspoil_check_field(psp, pfp, "unused")) {
		ret = PROCESS_STRING(psp, pfp, btt_info_backup.unused);
	} else if (pmemspoil_check_field(psp, pfp, "checksum")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info_backup.checksum);
	} else {
		out_err("unknown field '%s'\n", pfp->cur->name);
		return -1;
	}

	if (!ret) {
		util_convert2le_btt_info(&btt_info_backup);

		if (pwrite(psp->fd, &btt_info_backup,
				sizeof (btt_info_backup), backup_offset) !=
				sizeof (btt_info_backup)) {
			return -1;
		}
	} else {
		out_err("parsing value '%s'\n", pfp->value);
		return -1;
	}

	return 0;
}

/*
 * pmemspoil_process_btt_info -- process btt_info fields
 */
static int
pmemspoil_process_btt_info(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	uint64_t arena_offset =
		pmemspoil_get_arena_offset(psp, pfp->head->index);

	if (!arena_offset)
		return -1;

	struct btt_info btt_info;

	if (pread(psp->fd, &btt_info, sizeof (btt_info), arena_offset) !=
		sizeof (btt_info)) {
		return -1;
	}

	util_convert2h_btt_info(&btt_info);

	int ret = 0;
	if (pmemspoil_check_field(psp, pfp, "sig")) {
		ret = PROCESS_STRING(psp, pfp, btt_info.sig);
	} else if (pmemspoil_check_field(psp, pfp, "parent_uuid")) {
		ret = PROCESS_STRING(psp, pfp, btt_info.parent_uuid);
	} else if (pmemspoil_check_field(psp, pfp, "flags")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.flags);
	} else if (pmemspoil_check_field(psp, pfp, "major")) {
		ret = PROCESS_UINT16(psp, pfp, btt_info.major);
	} else if (pmemspoil_check_field(psp, pfp, "minor")) {
		ret = PROCESS_UINT16(psp, pfp, btt_info.minor);
	} else if (pmemspoil_check_field(psp, pfp, "external_lbasize")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.external_lbasize);
	} else if (pmemspoil_check_field(psp, pfp, "external_nlba")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.external_nlba);
	} else if (pmemspoil_check_field(psp, pfp, "internal_lbasize")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.internal_lbasize);
	} else if (pmemspoil_check_field(psp, pfp, "internal_nlba")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.internal_nlba);
	} else if (pmemspoil_check_field(psp, pfp, "nfree")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.nfree);
	} else if (pmemspoil_check_field(psp, pfp, "infosize")) {
		ret = PROCESS_UINT32(psp, pfp, btt_info.infosize);
	} else if (pmemspoil_check_field(psp, pfp, "nextoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.nextoff);
	} else if (pmemspoil_check_field(psp, pfp, "dataoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.dataoff);
	} else if (pmemspoil_check_field(psp, pfp, "mapoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.mapoff);
	} else if (pmemspoil_check_field(psp, pfp, "flogoff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.flogoff);
	} else if (pmemspoil_check_field(psp, pfp, "infooff")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.infooff);
	} else if (pmemspoil_check_field(psp, pfp, "unused")) {
		ret = PROCESS_STRING(psp, pfp, btt_info.unused);
	} else if (pmemspoil_check_field(psp, pfp, "checksum")) {
		ret = PROCESS_UINT64(psp, pfp, btt_info.checksum);
	} else {
		out_err("unknown field '%s'\n", pfp->cur->name);
		return -1;
	}

	if (ret) {
		out_err("parsing value '%s'\n", pfp->value);
		return -1;
	}

	util_convert2le_btt_info(&btt_info);

	if (pwrite(psp->fd, &btt_info, sizeof (btt_info), arena_offset) !=
		sizeof (btt_info)) {
		return -1;
	}

	return 0;
}

/*
 * pmemspoil_process_btt_map -- process btt map fields
 */
static int
pmemspoil_process_btt_map(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	uint64_t arena_offset =
		pmemspoil_get_arena_offset(psp, pfp->head->index);

	if (!arena_offset)
		return -1;

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
pmemspoil_process_btt_flog(struct pmemspoil *psp,
		struct pmemspoil_list *pfp, int off)
{
	uint64_t arena_offset =
		pmemspoil_get_arena_offset(psp, pfp->head->index);

	if (!arena_offset)
		return -1;

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

	if (pmemspoil_check_field(psp, pfp, "lba")) {
		ret = PROCESS_UINT32(psp, pfp, btt_flog.lba);
	} else if (pmemspoil_check_field(psp, pfp, "old_map")) {
		ret = PROCESS_UINT32(psp, pfp, btt_flog.old_map);
	} else if (pmemspoil_check_field(psp, pfp, "new_map")) {
		ret = PROCESS_UINT32(psp, pfp, btt_flog.new_map);
	} else if (pmemspoil_check_field(psp, pfp, "seq")) {
		ret = PROCESS_UINT32(psp, pfp, btt_flog.seq);
	} else {
		outv(1, "invalid field\n");
		ret = -1;
		goto error;
	}

	if (!ret) {
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
	} else {
		out_err("parsing value '%s'\n", pfp->value);
	}
error:
	free(flogp);

	return ret;
}

/*
 * pmemspoil_process_arena -- process arena fields
 */
static int
pmemspoil_process_arena(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	if (pmemspoil_check_field(psp, pfp, "btt_info")) {
		return pmemspoil_process_btt_info(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "btt_info_backup")) {
		return pmemspoil_process_btt_info_backup(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "btt_map")) {
		return pmemspoil_process_btt_map(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "btt_flog")) {
		return pmemspoil_process_btt_flog(psp, pfp, 0);
	} else if (pmemspoil_check_field(psp, pfp, "btt_flog_prime")) {
		return pmemspoil_process_btt_flog(psp, pfp, 1);
	}

	out_err("unknown header\n");
	return -1;
}

/*
 * pmemspoil_process -- process headers
 */
static int
pmemspoil_process(struct pmemspoil *psp,
		struct pmemspoil_list *pfp)
{
	if (pmemspoil_check_field(psp, pfp, "pool_hdr")) {
		return pmemspoil_process_pool_hdr(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "pmemlog")) {
		return pmemspoil_process_pmemlog(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "pmemblk")) {
		return pmemspoil_process_pmemblk(psp, pfp);
	} else if (pmemspoil_check_field(psp, pfp, "arena")) {
		return pmemspoil_process_arena(psp, pfp);
	}

	out_err("unknown header\n");
	return -1;
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
