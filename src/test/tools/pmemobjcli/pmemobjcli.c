// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */
/*
 * pmemobjcli.c -- CLI interface for pmemobj API
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>
#include <limits.h>
#include <setjmp.h>
#include <inttypes.h>
#include <getopt.h>

#include <libpmemobj.h>
#include "common.h"
#include "os.h"
#include "vec.h"

#define POCLI_ENV_EXIT_ON_ERROR	"PMEMOBJCLI_EXIT_ON_ERROR"
#define POCLI_ENV_ECHO_MODE	"PMEMOBJCLI_ECHO_MODE"
#define POCLI_ENV_COMMENTS	"PMEMOBJCLI_COMMENTS"
#define POCLI_ENV_EMPTY_CMDS	"PMEMOBJCLI_EMPTY_CMDS"
#define POCLI_ENV_LONG_NAMES	"PMEMOBJCLI_LONG_NAMES"
#define POCLI_ENV_HELP		"PMEMOBJCLI_HELP"
#define POCLI_CMD_DELIM		" "
#define POCLI_CMD_PROMPT	"pmemobjcli $ "
#define POCLI_INBUF_LEN		4096
struct pocli;

TOID_DECLARE(struct item, 1);

/*
 * item -- structure used to connect elements in lists.
 */
struct item {
	POBJ_LIST_ENTRY(struct item) field;
};

/*
 * plist -- structure used as a list entry.
 */
POBJ_LIST_HEAD(plist, struct item);

/*
 * struct pocli_ctx -- pmemobjcli context structure for commands
 */
struct pocli_ctx {
	PMEMobjpool *pop;
	PMEMoid root;
	FILE *err;
	FILE *out;
	struct pocli *pocli;
	bool tx_aborted;
	VEC(, struct pocli_args *) free_on_abort;
};

/*
 * struct pocli_args -- arguments for pmemobjcli command
 */
struct pocli_args {
	int argc;
	char *argv[];
};

/*
 * enum pocli_ret -- return values
 */
enum pocli_ret {
	POCLI_RET_OK,
	POCLI_ERR_ARGS,
	POCLI_ERR_PARS,
	POCLI_ERR_CMD,
	POCLI_ERR_MALLOC,
	POCLI_RET_QUIT,
};

/*
 * pocli_cmd_fn -- function prototype for pmemobjcli commands
 */
typedef enum pocli_ret (*pocli_cmd_fn)(struct pocli_ctx *ctx,
		struct pocli_args *args);

/*
 * struct pocli_cmd -- pmemobjcli command descriptor
 */
struct pocli_cmd {
	const char *name;	/* long name of command */
	const char *name_short;	/* short name of command */
	const char *usage;	/* usage string */
	pocli_cmd_fn func;	/* command's entry point */
};

/*
 * struct pocli_opts -- configuration options for pmemobjcli
 */
struct pocli_opts {
	bool exit_on_error;		/* exit when error occurred */
	bool echo_mode;			/* print every command from input */
	bool enable_comments;		/* enable comments on input */
	bool enable_empty_cmds;		/* enable empty lines */
	bool enable_long_names;		/* enable long names */
	bool enable_help;		/* enable printing help */
};

/*
 * struct pocli -- main context of pmemobjcli
 */
struct pocli {
	FILE *in;			/* input file handle */
	const char *fname;		/* pool's file name */
	char *inbuf;			/* input buffer */
	size_t inbuf_len;		/* input buffer length */
	struct pocli_ctx ctx;		/* context for commands */
	const struct pocli_cmd *cmds;	/* available commands */
	size_t ncmds;			/* number of available commands */
	int istty;			/* stdout is tty */
	struct pocli_opts opts;		/* configuration options */
	bool print_only;		/* don't execute, just print */
};

int pocli_process(struct pocli *pcli);

/*
 * pocli_err -- print error message
 */
static enum pocli_ret
pocli_err(struct pocli_ctx *ctx, enum pocli_ret ret, const char *fmt, ...)
{
	fprintf(ctx->err, "error: ");
	va_list ap;
	va_start(ap, fmt);
	vfprintf(ctx->err, fmt, ap);
	va_end(ap);
	return ret;
}

/*
 * pocli_printf -- print message
 */
static void
pocli_printf(struct pocli_ctx *ctx, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(ctx->out, fmt, ap);
	va_end(ap);
}

/*
 * pocli_args_number -- parse type number
 */
static enum pocli_ret
pocli_args_number(struct pocli_args *args, int arg, uint64_t *type_num)
{
	assert(args != NULL);
	assert(arg >= 0 && arg < args->argc);
	assert(type_num != NULL);

	uint64_t tn;
	char c;
	int ret = sscanf(args->argv[arg], "%" SCNu64 "%c", &tn, &c);
	if (ret != 1)
		return POCLI_ERR_PARS;

	*type_num = tn;

	return POCLI_RET_OK;
}

/*
 * pocli_args_size -- parse size
 */
static enum pocli_ret
pocli_args_size(struct pocli_args *args, int arg, size_t *sizep)
{
	assert(args != NULL);
	assert(arg >= 0 && arg < args->argc);
	assert(sizep != NULL);

	if (util_parse_size(args->argv[arg], sizep))
		return POCLI_ERR_PARS;

	return POCLI_RET_OK;
}

/*
 * pocli_args_alloc -- split line into array of arguments
 */
static struct pocli_args *
pocli_args_alloc(char *cmdstr, char *argstr, char *delim)
{
	size_t size = sizeof(struct pocli_args);
	struct pocli_args *args = NULL;
	if (cmdstr) {
		size += sizeof(char *);
		args = (struct pocli_args *)malloc(size);
		if (!args)
			return NULL;
		args->argc = 1;
		args->argv[0] = cmdstr;
	}

	char *n = strtok(argstr, delim);
	while (n) {
		int cur = args ? args->argc++ : 0;
		size += sizeof(char *);
		struct pocli_args *nargs =
			(struct pocli_args *)realloc(args, size);
		if (!nargs) {
			free(args);
			return NULL;
		}
		if (!args)
			nargs->argc = 1;
		args = nargs;
		args->argv[cur] = n;
		n = strtok(NULL, delim);
	}

	return args;
}

/*
 * pocli_args_obj_root -- parse object's descriptor from root object
 */
static enum pocli_ret
pocli_args_obj_root(struct pocli_ctx *ctx, char *in, PMEMoid **oidp)
{
	char *input = strdup(in);
	if (!input)
		return POCLI_ERR_MALLOC;

	enum pocli_ret ret = POCLI_ERR_PARS;

	if (!oidp)
		goto out_input;

	struct pocli_args *args = pocli_args_alloc(NULL, input, ".");
	if (!args)
		goto out_input;

	if (strcmp(args->argv[0], "r") != 0)
		goto out;

	PMEMoid *oid = &ctx->root;
	size_t size = pmemobj_root_size(ctx->pop);
	for (int i = 1; i < args->argc; i++) {

		unsigned ind;
		char c;
		int n = sscanf(args->argv[i], "%u%c", &ind, &c);
		if (n != 1)
			goto out;

		size_t max_ind = size / sizeof(PMEMoid);
		if (!max_ind || ind >= max_ind)
			goto out;

		PMEMoid *oids = (PMEMoid *)pmemobj_direct(*oid);
		oid = &oids[ind];
		size = pmemobj_alloc_usable_size(*oid);
	}

	ret = POCLI_RET_OK;

	*oidp = oid;

out:
	free(args);
out_input:
	free(input);
	return ret;
}

/*
 * pocli_args_obj -- parse object's descriptor
 */
static enum pocli_ret
pocli_args_obj(struct pocli_ctx *ctx, struct pocli_args *args,
		int arg, PMEMoid **oidp)
{
	assert(args != NULL);
	assert(arg >= 0 && arg < args->argc);
	assert(oidp != NULL);
	assert(ctx != NULL);

	char *objs = args->argv[arg];

	if (strcmp(objs, "r") == 0) {
		*oidp = &ctx->root;
	} else if (strcmp(objs, "0") == 0) {
		*oidp = NULL;
	} else if (strcmp(objs, "NULL") == 0) {
		*oidp = NULL;
	} else if (objs[0] == 'r') {
		return pocli_args_obj_root(ctx, args->argv[arg], oidp);
	} else {
		return pocli_err(ctx, POCLI_ERR_PARS,
			"invalid object specified -- '%s'\n", objs);
	}

	return POCLI_RET_OK;
}

/*
 * pocli_args_list_elm -- parse object's descriptor and checks if it's on list
 */
static enum pocli_ret
pocli_args_list_elm(struct pocli_ctx *ctx, struct pocli_args *args,
		int arg, PMEMoid **oidp, struct plist *head)
{
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, arg, oidp);
	if (ret)
		return ret;
	if (*oidp == NULL)
		return POCLI_RET_OK;
	TOID(struct item) tmp;
	POBJ_LIST_FOREACH(tmp, head, field) {
		if (OID_EQUALS(tmp.oid, **oidp))
			return POCLI_RET_OK;
	}

	return pocli_err(ctx, POCLI_ERR_PARS,
		"object %s is not member of given list\n", args->argv[arg]);
}

/*
 * parse_stage -- return proper string variable referring to transaction state
 */
static const char *
parse_stage(void)
{
	enum pobj_tx_stage st = pmemobj_tx_stage();
	const char *stage = "";
	switch (st) {
		case TX_STAGE_NONE:
			stage = "TX_STAGE_NONE";
		break;
		case TX_STAGE_WORK:
			stage = "TX_STAGE_WORK";
		break;
		case TX_STAGE_ONCOMMIT:
			stage = "TX_STAGE_ONCOMMIT";
		break;
		case TX_STAGE_ONABORT:
			stage = "TX_STAGE_ONABORT";
		break;
		case TX_STAGE_FINALLY:
			stage = "TX_STAGE_FINALLY";
		break;
		default:
			assert(0); /* unreachable */
		break;
	}
	return stage;
}

/*
 * pocli_pmemobj_direct -- pmemobj_direct() command
 */
static enum pocli_ret
pocli_pmemobj_direct(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	void *obj = pmemobj_direct(*oidp);

	pocli_printf(ctx, "%s(%s): off = 0x%jx uuid = 0x%jx ptr = %p\n",
			args->argv[0], args->argv[1],
			oidp->off, oidp->pool_uuid_lo, obj);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_type_num -- pmemobj_type_num() command
 */
static enum pocli_ret
pocli_pmemobj_type_num(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	uint64_t type_num = pmemobj_type_num(*oidp);

	pocli_printf(ctx, "%s(%s): type num = %llu\n",
			args->argv[0], args->argv[1], type_num);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_alloc_usable_size -- pmemobj_alloc_usable_size() command
 */
static enum pocli_ret
pocli_pmemobj_alloc_usable_size(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	size_t size = pmemobj_alloc_usable_size(*oidp);

	pocli_printf(ctx, "%s(%s): size = %zu\n",
			args->argv[0], args->argv[1], size);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_root -- pmemobj_root() command
 */
static enum pocli_ret
pocli_pmemobj_root(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	size_t size = 0;
	enum pocli_ret ret;
	ret = pocli_args_size(args, 1, &size);
	if (ret)
		return ret;

	PMEMoid root = pmemobj_root(ctx->pop, size);

	if (OID_IS_NULL(root))
		return pocli_err(ctx, POCLI_ERR_CMD, "pmemobj_root failed\n");

	ctx->root = root;

	pocli_printf(ctx, "%s(%zu): off = 0x%jx uuid = 0x%jx\n",
			args->argv[0], size, ctx->root.off,
			ctx->root.pool_uuid_lo);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_root_size -- pmemobj_root_size() command
 */
static enum pocli_ret
pocli_pmemobj_root_size(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	size_t size = pmemobj_root_size(ctx->pop);

	pocli_printf(ctx, "%s: size = %lu\n",
			args->argv[0], size);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_do_alloc -- pmemobj_alloc and pmemobj_zalloc() common part
 */
static enum pocli_ret
pocli_pmemobj_do_alloc(struct pocli_ctx *ctx, struct pocli_args *args,
	int (fn_alloc)(PMEMobjpool *pop, PMEMoid *oid, size_t size,
							uint64_t type_num))
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	uint64_t type_num = 0;
	size_t size = 0;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"cannot allocate to root object\n");

	ret = pocli_args_number(args, 2, &type_num);
	if (ret)
		return ret;

	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;

	int r = fn_alloc(ctx->pop, oidp, size, type_num);

	pocli_printf(ctx, "%s(%s, %zu, %llu): %d\n",
		args->argv[0], args->argv[1], size, type_num, r);

	return ret;
}

/*
 * do_alloc -- wrapper for pmemobj_alloc() function with default constructor.
 */
static int
do_alloc(PMEMobjpool *pop, PMEMoid *oidp, size_t size, uint64_t type_num)
{
	return pmemobj_alloc(pop, oidp, size, type_num, NULL, NULL);
}

/*
 * pocli_pmemobj_alloc -- pmemobj_alloc() command
 */
static enum pocli_ret
pocli_pmemobj_alloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_alloc(ctx, args, do_alloc);
}

/*
 * pocli_pmemobj_zalloc -- pmemobj_zalloc() command
 */
static enum pocli_ret
pocli_pmemobj_zalloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_alloc(ctx, args, pmemobj_zalloc);
}

/*
 * pocli_pmemobj_do_realloc -- pmemobj_realloc and pmemobj_zrealloc() commands
 * common part
 */
static enum pocli_ret
pocli_pmemobj_do_realloc(struct pocli_ctx *ctx, struct pocli_args *args,
	int (*fn_realloc)(PMEMobjpool *pop, PMEMoid *oid, size_t size,
							uint64_t type_num))
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	uint64_t type_num = 0;
	size_t size = 0;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"cannot realloc with NULL oid pointer\n");

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"cannot reallocate to root object\n");

	ret = pocli_args_number(args, 2, &type_num);
	if (ret)
		return ret;

	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;

	int r = fn_realloc(ctx->pop, oidp, size, type_num);

	pocli_printf(ctx, "%s(%s, %zu, %llu): %d off = 0x%llx uuid = 0x%llx\n",
				args->argv[0], args->argv[1], size, type_num,
				r, oidp->off, oidp->pool_uuid_lo);

	return ret;
}

/*
 * pocli_pmemobj_realloc -- pmemobj_realloc() command
 */
static enum pocli_ret
pocli_pmemobj_realloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_realloc(ctx, args, pmemobj_realloc);
}

/*
 * pocli_pmemobj_zrealloc -- pmemobj_zrealloc() command
 */
static enum pocli_ret
pocli_pmemobj_zrealloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_realloc(ctx, args, pmemobj_zrealloc);
}

/*
 * pocli_pmemobj_free -- pmemobj_free() command
 */
static enum pocli_ret
pocli_pmemobj_free(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"NULL pointer not allowed here\n");
	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"cannot free root object\n");

	void *oidp_tmp = pmemobj_direct(*oidp);
	pmemobj_free(oidp);

	pocli_printf(ctx, "%s(%p): off = 0x%llx uuid = 0x%llx\n",
		args->argv[0], oidp_tmp, oidp->off, oidp->pool_uuid_lo);

	return ret;
}

/*
 * pocli_pmemobj_strdup -- pmemobj_strdup() command
 */
static enum pocli_ret
pocli_pmemobj_strdup(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	uint64_t type_num;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;
	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot use root object\n");

	ret = pocli_args_number(args, 3, &type_num);
	if (ret)
		return ret;

	int r = pmemobj_strdup(ctx->pop, oidp, args->argv[2], type_num);
	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_strdup() failed\n");

	pocli_printf(ctx, "%s(%s, %s, %llu): %d\n",
				args->argv[0], args->argv[1], args->argv[2],
								type_num, r);
	return ret;
}

/*
 * pocli_str_root_copy -- copy a string into a root object data
 */
static enum pocli_ret
pocli_str_root_copy(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 3)
		return POCLI_ERR_ARGS;

	size_t offset = 0;
	enum pocli_ret ret = pocli_args_size(args, 1, &offset);
	if (ret)
		return ret;

	const char *str = args->argv[2];
	if (str == NULL)
		return POCLI_ERR_ARGS;

	size_t len = strlen(str);

	size_t root_size = pmemobj_root_size(ctx->pop);
	if (offset + len > root_size)
		return POCLI_ERR_ARGS;

	PMEMoid root = pmemobj_root(ctx->pop, root_size);
	assert(!OID_IS_NULL(root));
	char *root_data = (char *)pmemobj_direct(root);
	pmemobj_memcpy_persist(ctx->pop, root_data + offset, str, len);
	return ret;
}

/*
 * pocli_str_root_print -- print a string stored in the root object data
 */
static enum pocli_ret
pocli_str_root_print(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 3)
		return POCLI_ERR_ARGS;

	size_t offset = 0;
	enum pocli_ret ret = pocli_args_size(args, 1, &offset);
	if (ret)
		return ret;

	size_t len = 0;
	ret = pocli_args_number(args, 2, &len);
	if (ret)
		return ret;

	size_t root_size = pmemobj_root_size(ctx->pop);
	if (offset + len > root_size)
		return POCLI_ERR_ARGS;

	PMEMoid root = pmemobj_root(ctx->pop, root_size);
	assert(!OID_IS_NULL(root));
	char *root_data = (char *)pmemobj_direct(root);

	char *buff = (char *)malloc(len + 1);
	assert(buff != NULL);
	memcpy(buff, root_data + offset, len);
	buff[len] = '\0';
	printf("%s\n", buff);
	free(buff);
	return ret;
}

/*
 * pocli_pmemobj_first -- pmemobj_first() command
 */
static enum pocli_ret
pocli_pmemobj_first(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	PMEMoid oidp = pmemobj_first(ctx->pop);
	if (OID_IS_NULL(oidp))
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_first() failed\n");

	pocli_printf(ctx, "%s: off = 0x%llx uuid = 0x%llx\n",
				args->argv[0], oidp.off, oidp.pool_uuid_lo);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_next -- pmemobj_next() command
 */
static enum pocli_ret
pocli_pmemobj_next(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	PMEMoid oidp_next;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	oidp_next = pmemobj_next(*oidp);

	pocli_printf(ctx, "%s(%p): off = 0x%llx uuid = 0x%llx\n",
			args->argv[0], pmemobj_direct(*oidp), oidp_next.off,
							oidp_next.pool_uuid_lo);

	return ret;
}

/*
 * pocli_pmemobj_memcpy_persist -- pmemobj_memcpy_persist() command
 */
static enum pocli_ret
pocli_pmemobj_memcpy_persist(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 6)
		return POCLI_ERR_ARGS;

	PMEMoid *dest = NULL;
	PMEMoid *src;
	enum pocli_ret ret;
	uint64_t offset;
	uint64_t len;

	if ((ret = pocli_args_obj(ctx, args, 1, &dest)))
		return ret;
	if ((ret = pocli_args_number(args, 2, &offset)))
		return ret;

	if (dest == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	char *dest_p = (char *)pmemobj_direct(*dest);
	dest_p += offset;

	if ((ret = pocli_args_obj(ctx, args, 3, &src)))
		return ret;
	if ((ret = pocli_args_number(args, 4, &offset)))
		return ret;

	if (src == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[3]);

	char *src_p = (char *)pmemobj_direct(*src);
	src_p += offset;

	if ((ret = pocli_args_number(args, 5, &len)))
		return ret;

	void *result = pmemobj_memcpy_persist(ctx->pop, dest_p, src_p, len);

	pocli_printf(ctx, "%s(%p, %p, %u): ptr = %p\n",
			args->argv[0], dest_p, src_p, len, result);

	return ret;
}

/*
 * pocli_pmemobj_memset_persist -- pmemobj_memset_persist() command
 */
static enum pocli_ret
pocli_pmemobj_memset_persist(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 5)
		return POCLI_ERR_ARGS;

	PMEMoid *oid = NULL;
	enum pocli_ret ret;
	uint64_t offset;
	uint64_t len;
	uint64_t c;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;
	if ((ret = pocli_args_number(args, 2, &offset)))
		return ret;

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	char *dest_p = (char *)pmemobj_direct(*oid);
	dest_p += offset;

	if ((ret = pocli_args_number(args, 3, &c)))
		return ret;
	if ((ret = pocli_args_number(args, 4, &len)))
		return ret;

	if (len == UINT64_MAX)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[4]);

	void *result = pmemobj_memset_persist(ctx->pop, dest_p, (int)c, len);

	pocli_printf(ctx, "%s(%p, %u, %d): ptr = %p\n",
			args->argv[0], dest_p, c, len, result);

	return ret;
}

/*
 * pocli_pmemobj_do_persist -- common part of pmemobj_persist() and
 * pmemobj_flush() command
 */
static enum pocli_ret
pocli_pmemobj_do_persist(struct pocli_ctx *ctx, struct pocli_args *args,
	void (*fn_persist)(PMEMobjpool *pop, const void *addr, size_t len))
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oid = NULL;
	enum pocli_ret ret;
	uint64_t offset;
	uint64_t len;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;
	if ((ret = pocli_args_number(args, 2, &offset)))
		return ret;

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	char *dest_p = (char *)pmemobj_direct(*oid);
	dest_p += offset;

	if ((ret = pocli_args_number(args, 3, &len)))
		return ret;

	fn_persist(ctx->pop, dest_p, len);

	pocli_printf(ctx, "%s(%p, %u)\n",
			args->argv[0], dest_p, len);

	return ret;
}

/*
 * pocli_pmemobj_persist -- pmemobj_persist() command
 */
static enum pocli_ret
pocli_pmemobj_persist(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_persist(ctx, args, pmemobj_persist);
}

/*
 * pocli_pmemobj_flush -- pmemobj_flush() command
 */
static enum pocli_ret
pocli_pmemobj_flush(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_persist(ctx, args, pmemobj_flush);
}

/*
 * pocli_pmemobj_drain -- pmemobj_drain() command
 */
static enum pocli_ret
pocli_pmemobj_drain(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	pmemobj_drain(ctx->pop);

	pocli_printf(ctx, "%s\n", args->argv[0]);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_pool_by_ptr -- pmemobj_pool_by_ptr() command
 */
static enum pocli_ret
pocli_pmemobj_pool_by_ptr(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 3)
		return POCLI_ERR_ARGS;

	PMEMoid *oid = NULL;
	enum pocli_ret ret;
	uint64_t offset;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;
	if ((ret = pocli_args_number(args, 2, &offset)))
		return ret;

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	char *dest_p = (char *)pmemobj_direct(*oid);
	dest_p += offset;

	PMEMobjpool *pop = pmemobj_pool_by_ptr(dest_p);

	pocli_printf(ctx, "%s(%p): uuid = 0x%llx\n",
			args->argv[0], dest_p, pop->uuid_lo);

	return ret;
}

/*
 * pocli_pmemobj_pool_by_oid -- pmemobj_pool_by_oid() command
 */
static enum pocli_ret
pocli_pmemobj_pool_by_oid(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	PMEMoid *oid = NULL;
	enum pocli_ret ret;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	PMEMobjpool *pop = pmemobj_pool_by_oid(*oid);

	pocli_printf(ctx, "%s(%p): uuid = 0x%llx\n",
			args->argv[0], pmemobj_direct(*oid), pop->uuid_lo);
	return ret;
}

/*
 * pocli_pmemobj_list_insert -- pmemobj_list_insert() command
 */
static enum pocli_ret
pocli_pmemobj_list_insert(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 5)
		return POCLI_ERR_ARGS;

	PMEMoid nulloid = OID_NULL;
	PMEMoid *dest;
	PMEMoid *oid = NULL;
	PMEMoid *head_oid;
	enum pocli_ret ret;
	uint64_t before;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;

	if (pocli_args_obj(ctx, args, 2, &head_oid))
		return ret;

	if (head_oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[2]);

	struct plist *head = (struct plist *)pmemobj_direct(*head_oid);

	if ((ret = pocli_args_list_elm(ctx, args, 3, &dest, head)))
		return ret;
	if (dest == NULL)
		dest = &nulloid;

	if ((ret = pocli_args_number(args, 4, &before)))
		return ret;
	if (before > 1)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"Before flag different than 0 or 1\n");

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	int r = pmemobj_list_insert(ctx->pop, offsetof(struct item, field),
						head, *dest, (int)before, *oid);
	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_list_insert() failed\n");
	pocli_printf(ctx, "%s(%p, %s, %p, %u): %d\n",
			args->argv[0], pmemobj_direct(*oid), args->argv[2],
			dest, before, r);

	return ret;
}

/*
 * pocli_pmemobj_list_insert_new -- pmemobj_list_insert_new() command
 */
static enum pocli_ret
pocli_pmemobj_list_insert_new(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 7)
		return POCLI_ERR_ARGS;

	PMEMoid nulloid = OID_NULL;
	PMEMoid *dest;
	PMEMoid *oid = NULL;
	PMEMoid *head_oid;
	enum pocli_ret ret;
	uint64_t before;
	uint64_t type_num;
	uint64_t size;

	if ((ret = pocli_args_obj(ctx, args, 1, &oid)))
		return ret;
	if (oid == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot allocate to root object\n");

	if ((ret = pocli_args_obj(ctx, args, 2, &head_oid)))
		return ret;

	if (head_oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[2]);

	struct plist *head = (struct plist *)pmemobj_direct(*head_oid);

	if ((ret = pocli_args_list_elm(ctx, args, 3, &dest, head)))
		return ret;
	if (dest == NULL)
		dest = &nulloid;

	if ((ret = pocli_args_number(args, 4, &before)))
		return ret;
	if (before > 1)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"Before flag different than 0 or 1\n");

	if ((ret = pocli_args_number(args, 5, &type_num)))
		return ret;
	if ((ret = pocli_args_number(args, 6, &size)))
		return ret;

	*oid = pmemobj_list_insert_new(ctx->pop, offsetof(struct item, field),
			head, *dest, (int)before, size, type_num, NULL, NULL);
	pmemobj_persist(ctx->pop, oid, sizeof(PMEMoid));

	if (OID_IS_NULL(*oid))
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_list_insert_new() failed\n");

	pocli_printf(ctx, "%s(%s, %p, %u, %llu, %zu): off = 0x%jx uuid = 0x%jx"
				" ptr = %p\n", args->argv[0], args->argv[2],
				dest, before, type_num, size, oid->off,
				oid->pool_uuid_lo, pmemobj_direct(*oid));

	return ret;
}

/*
 * pocli_pmemobj_list_remove -- pmemobj_list_remove() command
 */
static enum pocli_ret
pocli_pmemobj_list_remove(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oid;
	PMEMoid *head_oid = NULL;
	enum pocli_ret ret;
	uint64_t if_free;

	if ((ret = pocli_args_obj(ctx, args, 2, &head_oid)))
		return ret;

	if (head_oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[2]);

	struct plist *head = (struct plist *)pmemobj_direct(*head_oid);

	if ((ret = pocli_args_list_elm(ctx, args, 1, &oid, head)))
		return ret;

	if ((ret = pocli_args_number(args, 3, &if_free)))
		return ret;
	if (if_free > 1)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"Free flag different than 0 or 1\n");

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	void *oidp =  pmemobj_direct(*oid);
	int r = pmemobj_list_remove(ctx->pop, offsetof(struct item, field),
						head, *oid, (int)if_free);
	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_list_remove() failed\n");

	if (if_free) {
		*oid = OID_NULL;
		pmemobj_persist(ctx->pop, oid, sizeof(PMEMoid));
	}

	pocli_printf(ctx, "%s(%p, %s, %u): off = 0x%jx uuid = 0x%jx\n",
				args->argv[0], oidp, args->argv[2], if_free,
				oid->off, oid->pool_uuid_lo);

	return ret;
}

/*
 * pocli_pmemobj_list_move -- pmemobj_list_move() command
 */
static enum pocli_ret
pocli_pmemobj_list_move(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 6)
		return POCLI_ERR_ARGS;

	PMEMoid nulloid = OID_NULL;
	PMEMoid *dest;
	PMEMoid *oid;
	PMEMoid *head_oid;
	enum pocli_ret ret;
	uint64_t before;
	size_t offset = offsetof(struct item, field);

	if ((ret = pocli_args_obj(ctx, args, 2, &head_oid)))
		return ret;

	if (head_oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[2]);

	struct plist *head_src = (struct plist *)pmemobj_direct(*head_oid);

	if ((ret = pocli_args_obj(ctx, args, 3, &head_oid)))
		return ret;

	if (head_oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[3]);

	struct plist *head_dest = (struct plist *)pmemobj_direct(*head_oid);

	if ((ret = pocli_args_list_elm(ctx, args, 1, &oid, head_src)))
		return ret;

	if ((ret = pocli_args_list_elm(ctx, args, 4, &dest, head_dest)))
		return ret;
	if (dest == NULL)
		dest = &nulloid;

	if ((ret = pocli_args_number(args, 5, &before)))
		return ret;
	if (before > 1)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"Before flag different than 0 or 1\n");

	if (oid == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	int r = pmemobj_list_move(ctx->pop, offset, head_src, offset, head_dest,
						*dest, (int)before, *oid);
	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_list_move() failed\n");

	pocli_printf(ctx, "%s(%p, %s, %s, %p, %u): %d\n", args->argv[0],
			pmemobj_direct(*oid), args->argv[2], args->argv[3],
			pmemobj_direct(*dest), before, r);

	return ret;
}

/*
 * pocli_pmemobj_tx_begin -- pmemobj_tx_begin() command
 */
static enum pocli_ret
pocli_pmemobj_tx_begin(struct pocli_ctx *ctx, struct pocli_args *args)
{
	enum pocli_ret ret = POCLI_RET_OK;
	int r;
	switch (args->argc) {
		case 1: {
			r = pmemobj_tx_begin(ctx->pop, NULL, TX_PARAM_NONE);
			if (r != POCLI_RET_OK)
				return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_tx_begin() failed");
			pocli_printf(ctx, "%s: %d\n", args->argv[0], r);
		}
		break;
		case 2: {
			if (strcmp(args->argv[1], "jmp") != 0)
				return POCLI_ERR_ARGS;
			jmp_buf jmp;
			if (setjmp(jmp)) {
				const char *command = ctx->tx_aborted ?
					"pmemobj_tx_abort" : "pmemobj_tx_end";
				pocli_printf(ctx, "%s: %d\n",
					command, pmemobj_tx_errno());

				/*
				 * Free all objects, except the one we currently
				 * use.
				 */
				while (VEC_SIZE(&ctx->free_on_abort) > 1) {
					free(VEC_BACK(&ctx->free_on_abort));
					VEC_POP_BACK(&ctx->free_on_abort);
				}

				return POCLI_RET_OK;
			} else {
				r = pmemobj_tx_begin(ctx->pop, jmp,
							TX_PARAM_NONE);
				if (r != POCLI_RET_OK)
					return pocli_err(ctx, POCLI_ERR_ARGS,
						"pmemobj_tx_begin() failed");
			}
			pocli_printf(ctx, "%s(jmp): %d\n", args->argv[0], r);
			ret = (enum pocli_ret)pocli_process(ctx->pocli);
			if (ret)
				return ret;

		}
		break;
		default:
			return POCLI_ERR_ARGS;
	}
	return ret;
}

/*
 * pocli_pmemobj_tx_end -- pmemobj_tx_end() command
 */
static enum pocli_ret
pocli_pmemobj_tx_end(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	if (pmemobj_tx_stage() == TX_STAGE_NONE ||
					pmemobj_tx_stage() == TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"transaction in improper stage\n");
	ctx->tx_aborted = false;
	int ret = pmemobj_tx_end();
	pocli_printf(ctx, "%s: %d\n", args->argv[0], ret);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_tx_commit -- pmemobj_tx_commit() command
 */
static enum pocli_ret
pocli_pmemobj_tx_commit(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");

	pmemobj_tx_commit();
	pocli_printf(ctx, "%s\n", args->argv[0]);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_tx_abort -- pmemobj_tx_abort() command
 */
static enum pocli_ret
pocli_pmemobj_tx_abort(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");
	int err;
	int count = sscanf(args->argv[1], "%d", &err);
	if (count != 1)
		return POCLI_ERR_PARS;

	ctx->tx_aborted = true;
	pmemobj_tx_abort(err);
	pocli_printf(ctx, "pmemobj_tx_abort: %d", err);

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_tx_stage -- pmemobj_tx_stage() command
 */
static enum pocli_ret
pocli_pmemobj_tx_stage(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	pocli_printf(ctx, "%s: %s\n", args->argv[0], parse_stage());

	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_tx_add_range -- pmemobj_tx_add_range() command
 */
static enum pocli_ret
pocli_pmemobj_tx_add_range(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	size_t offset = 0;
	size_t size = 0;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;
	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
						"cannot add NULL pointer\n");

	ret = pocli_args_size(args, 2, &offset);
	if (ret)
		return ret;
	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;
	int r = pmemobj_tx_add_range(*oidp, offset, size);

	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_tx_add_range() failed");

	pocli_printf(ctx, "%s(%s, %zu, %zu): %d\n", args->argv[0],
					args->argv[1], offset, size, ret, r);

	return ret;
}

/*
 * pocli_pmemobj_tx_add_range_direct -- pmemobj_tx_add_range_direct() command
 */
static enum pocli_ret
pocli_pmemobj_tx_add_range_direct(struct pocli_ctx *ctx,
						struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	size_t off = 0;
	size_t size = 0;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;
	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
						"cannot add NULL pointer\n");
	char *ptr = (char *)pmemobj_direct(*oidp);

	ret = pocli_args_size(args, 2, &off);
	if (ret)
		return ret;

	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;

	int r = pmemobj_tx_add_range_direct((void *)(ptr + off), size);

	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_tx_add_range_direct() failed");

	pocli_printf(ctx, "%s(%p, %zu, %zu): %d\n", args->argv[0], ptr,
								off, size, r);

	return ret;
}

/*
 * pocli_pmemobj_do_tx_alloc -- pmemobj_tx_zalloc() and pmemobj_tx_zalloc()
 * commands common part
 */
static enum pocli_ret
pocli_pmemobj_do_tx_alloc(struct pocli_ctx *ctx, struct pocli_args *args,
			PMEMoid (*fn_alloc)(size_t size, uint64_t type_num))
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;
	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");
	PMEMoid *oidp = NULL;
	uint64_t type_num = 0;
	size_t size = 0;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot allocate to root object\n");

	ret = pocli_args_size(args, 2, &size);
	if (ret)
		return ret;

	ret = pocli_args_number(args, 3, &type_num);
	if (ret)
		return ret;
	*oidp = fn_alloc(size, type_num);

	pocli_printf(ctx, "%s(%zu, %llu): off = 0x%llx uuid = 0x%llx\n",
		args->argv[0], size, type_num, oidp->off, oidp->pool_uuid_lo);
	return ret;
}

/*
 * pocli_pmemobj_tx_alloc -- pmemobj_tx_alloc() command
 */
static enum pocli_ret
pocli_pmemobj_tx_alloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_tx_alloc(ctx, args, pmemobj_tx_alloc);
}
/*
 * pocli_pmemobj_tx_zalloc -- pmemobj_tx_zalloc() command
 */
static enum pocli_ret
pocli_pmemobj_tx_zalloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_tx_alloc(ctx, args, pmemobj_tx_zalloc);
}

/*
 * pocli_pmemobj_do_tx_realloc -- pmemobj_tx_zrealloc() and
 * pmemobj_tx_zrealloc() commands common part
 */
static enum pocli_ret
pocli_pmemobj_do_tx_realloc(struct pocli_ctx *ctx, struct pocli_args *args,
	PMEMoid (*fn_realloc)(PMEMoid oid, size_t size, uint64_t type_num))
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;
	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");
	PMEMoid *oidp = NULL;
	uint64_t type_num = 0;
	size_t size = 0;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot reallocate root object\n");

	ret = pocli_args_size(args, 2, &size);
	if (ret)
		return ret;

	ret = pocli_args_number(args, 3, &type_num);
	if (ret)
		return ret;
	*oidp = fn_realloc(*oidp, size, type_num);

	pocli_printf(ctx, "%s(%p, %zu, %llu): off = 0x%llx uuid = 0x%llx\n",
				args->argv[0], oidp, size, type_num,
				oidp->off, oidp->pool_uuid_lo);
	return ret;
}

/*
 * pocli_pmemobj_tx_realloc -- pmemobj_tx_realloc() command
 */
static enum pocli_ret
pocli_pmemobj_tx_realloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_tx_realloc(ctx, args, pmemobj_tx_realloc);
}

/*
 * pocli_pmemobj_tx_zrealloc -- pmemobj_tx_zrealloc() command
 */
static enum pocli_ret
pocli_pmemobj_tx_zrealloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	return pocli_pmemobj_do_tx_realloc(ctx, args, pmemobj_tx_zrealloc);
}

/*
 * pocli_pmemobj_tx_free -- pmemobj_tx_free() command
 */
static enum pocli_ret
pocli_pmemobj_tx_free(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 2)
		return POCLI_ERR_ARGS;
	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");

	PMEMoid *oidp = NULL;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot free root object\n");

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	int r = pmemobj_tx_free(*oidp);
	if (r != POCLI_RET_OK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"pmemobj_tx_free() failed\n");
	else
		*oidp = OID_NULL;

	pocli_printf(ctx, "%s(%p): off = 0x%llx uuid = 0x%llx\n",
				args->argv[0], oidp,
				oidp->off, oidp->pool_uuid_lo);
	return ret;
}

/*
 * pocli_pmemobj_tx_strdup -- pmemobj_tx_strdup() command
 */
static enum pocli_ret
pocli_pmemobj_tx_strdup(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;
	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"cannot use in stage different than TX_STAGE_WORK\n");

	PMEMoid *oidp = NULL;
	uint64_t type_num;
	enum pocli_ret ret;
	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;
	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
					"cannot use root object\n");

	ret = pocli_args_number(args, 3, &type_num);
	if (ret)
		return ret;

	if (oidp == NULL)
		return pocli_err(ctx, POCLI_ERR_ARGS,
			"invalid object -- '%s'\n", args->argv[1]);

	*oidp = pmemobj_tx_strdup(args->argv[2], type_num);

	pocli_printf(ctx, "%s(%s, %llu): off = 0x%llx uuid = 0x%llx\n",
				args->argv[0], args->argv[2], type_num,
				oidp->off, oidp->pool_uuid_lo);
	return ret;
}

/*
 * pocli_pmemobj_tx_process -- pmemobj_tx_process() command
 */
static enum pocli_ret
pocli_pmemobj_tx_process(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	pmemobj_tx_process();
	pocli_printf(ctx, "%s\n", args->argv[0]);
	return POCLI_RET_OK;
}

/*
 * pocli_pmemobj_tx_errno -- pmemobj_tx_errno() command
 */
static enum pocli_ret
pocli_pmemobj_tx_errno(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	pocli_printf(ctx, "%s: %d\n", args->argv[0], pmemobj_tx_errno());
	return POCLI_RET_OK;
}

/*
 * pocli_get_cmd -- find command of given name
 */
static const struct pocli_cmd *
pocli_get_cmd(struct pocli *pcli, const char *cmds)
{
	for (size_t i = 0; i < pcli->ncmds; i++) {
		const char *name = pcli->cmds[i].name;
		const char *name_short = pcli->cmds[i].name_short;
		if (strcmp(cmds, name_short) == 0 ||
			(pcli->opts.enable_long_names &&
			strcmp(cmds, name) == 0)) {
			return &pcli->cmds[i];
		}
	}

	return NULL;
}

static enum pocli_ret
pocli_print(struct pocli_ctx *ctx, struct pocli_args *args)
{
	const struct pocli_cmd *cmd = pocli_get_cmd(ctx->pocli, args->argv[0]);
	if (!cmd)
		return POCLI_ERR_PARS;

	printf("%s", cmd->name_short);
	if (args->argc == 1) {
		printf("\n");
		return POCLI_RET_OK;
	}

	for (int i = 1; i < args->argc; i++)
		printf(" %s", args->argv[i]);
	printf("\n");

	return POCLI_RET_OK;
}

static struct pocli_cmd print_cmd = {"", "", "", pocli_print};

/*
 * pocli_print_cmd -- print description of specified command
 */
static void
pocli_print_cmd(struct pocli_ctx *ctx, const struct pocli_cmd *cmd)
{
	pocli_printf(ctx, "[%-5s] %-32s - usage: %s %s\n",
		cmd->name_short,
		cmd->name,
		cmd->name,
		cmd->usage);
}

/*
 * pocli_print_cmd_usage -- print usage of specified command
 */
static void
pocli_print_cmd_usage(struct pocli_ctx *ctx, const struct pocli_cmd *cmd)
{
	pocli_printf(ctx, "usage: %s %s\n",
		cmd->name,
		cmd->usage);
}

/*
 * pocli_help -- help command
 */
static enum pocli_ret
pocli_help(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (!ctx->pocli->opts.enable_help)
		return POCLI_ERR_CMD;

	if (args->argc != 2 && args->argc != 1)
		return POCLI_ERR_ARGS;

	if (args->argc == 1) {
		for (size_t i = 0; i < ctx->pocli->ncmds; i++)
			pocli_print_cmd(ctx, &ctx->pocli->cmds[i]);
	} else {
		const struct pocli_cmd *cmd =
			pocli_get_cmd(ctx->pocli, args->argv[1]);
		if (!cmd)
			return POCLI_ERR_PARS;
		pocli_print_cmd_usage(ctx, cmd);
	}

	return POCLI_RET_OK;
}

/*
 * pocli_quit -- quit command
 */
static enum pocli_ret
pocli_quit(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 1)
		return POCLI_ERR_ARGS;

	return POCLI_RET_QUIT;
}

/*
 * pocli_commands -- list of available commands
 */
static struct pocli_cmd pocli_commands[] = {
	{
		"help",		/* name */
		"h",		/* name_short */
		"[<cmd>]",	/* usage */
		pocli_help,	/* func */
	},
	{
		"quit",
		"q",
		"",
		pocli_quit,
	},
	{
		"pmemobj_root",
		"pr",
		"<size>",
		pocli_pmemobj_root,
	},
	{
		"pmemobj_root_size",
		"prs",
		"",
		pocli_pmemobj_root_size,
	},
	{
		"pmemobj_direct",
		"pdr",
		"<obj>",
		pocli_pmemobj_direct,
	},
	{
		"pmemobj_alloc_usable_size",
		"paus",
		"<obj>",
		pocli_pmemobj_alloc_usable_size,
	},
	{
		"pmemobj_alloc",
		"pa",
		"<obj> <type_num> <size>",
		pocli_pmemobj_alloc,
	},
	{
		"pmemobj_zalloc",
		"pza",
		"<obj> <type_num> <size>",
		pocli_pmemobj_zalloc,
	},
	{
		"pmemobj_realloc",
		"pre",
		"<obj> <type_num> <size>",
		pocli_pmemobj_realloc,
	},
	{
		"pmemobj_zrealloc",
		"pzre",
		"<obj> <type_num> <size>",
		pocli_pmemobj_zrealloc,
	},
	{
		"pmemobj_free",
		"pf",
		"<obj>",
		pocli_pmemobj_free,
	},
	{
		"pmemobj_type_num",
		"ptn",
		"<obj>",
		pocli_pmemobj_type_num,
	},
	{
		"pmemobj_strdup",
		"psd",
		"<obj> <string> <type_num>",
		pocli_pmemobj_strdup,
	},
	{
		"pmemobj_first",
		"pfi",
		"<type_num>",
		pocli_pmemobj_first,
	},
	{
		"pmemobj_next",
		"pn",
		"<obj>",
		pocli_pmemobj_next,
	},
	{
		"pmemobj_memcpy_persist",
		"pmcp",
		"<dest> <off_dest> <src> <off_src> <len>",
		pocli_pmemobj_memcpy_persist,
	},
	{
		"pmemobj_memset_persist",
		"pmsp",
		"<obj> <offset> <pattern> <len>",
		pocli_pmemobj_memset_persist,
	},
	{
		"pmemobj_persist",
		"pp",
		"<obj> <offset> <len>",
		pocli_pmemobj_persist,
	},
	{
		"pmemobj_flush",
		"pfl",
		"<obj> <offset> <len>",
		pocli_pmemobj_flush,
	},
	{
		"pmemobj_drain",
		"pd",
		"",
		pocli_pmemobj_drain,
	},
	{
		"pmemobj_pool_by_oid",
		"ppbo",
		"<obj>",
		pocli_pmemobj_pool_by_oid,
	},
	{
		"pmemobj_pool_by_ptr",
		"ppbp",
		"<obj> <offset>",
		pocli_pmemobj_pool_by_ptr,
	},
	{
		"pmemobj_list_insert",
		"pli",
		"<obj> <head> <dest> <before>",
		pocli_pmemobj_list_insert,
	},
	{
		"pmemobj_list_insert_new",
		"plin",
		"<obj> <head> <dest> <before>"
							" <size> <type_num>",
		pocli_pmemobj_list_insert_new,
	},
	{
		"pmemobj_list_remove",
		"plr",
		"<obj> <head> <free>",
		pocli_pmemobj_list_remove,
	},
	{
		"pmemobj_list_move",
		"plm",
		"<obj> <head_src> <head_dest> "
							"<dest> <before>",
		pocli_pmemobj_list_move,
	},
	{
		"pmemobj_tx_begin",
		"ptb",
		"[<jmp>]",
		pocli_pmemobj_tx_begin,
	},
	{
		"pmemobj_tx_end",
		"pte",
		"",
		pocli_pmemobj_tx_end,
	},
	{
		"pmemobj_tx_abort",
		"ptab",
		"<errnum>",
		pocli_pmemobj_tx_abort,
	},
	{
		"pmemobj_tx_commit",
		"ptc",
		"",
		pocli_pmemobj_tx_commit,
	},
	{
		"pmemobj_tx_stage",
		"pts",
		"",
		pocli_pmemobj_tx_stage,
	},
	{
		"pmemobj_tx_add_range",
		"ptar",
		"<obj> <offset> <size>",
		pocli_pmemobj_tx_add_range,
	},
	{
		"pmemobj_tx_add_range_direct",
		"ptard",
		"<obj> <offset> <size>",
		pocli_pmemobj_tx_add_range_direct,
	},
	{
		"pmemobj_tx_process",
		"ptp",
		"",
		pocli_pmemobj_tx_process,
	},
	{
		"pmemobj_tx_alloc",
		"ptal",
		"<obj> <size> <type_num>",
		pocli_pmemobj_tx_alloc,
	},
	{
		"pmemobj_tx_zalloc",
		"ptzal",
		"<obj> <size> <type_num>",
		pocli_pmemobj_tx_zalloc,
	},
	{
		"pmemobj_tx_realloc",
		"ptre",
		"<obj> <size> <type_num>",
		pocli_pmemobj_tx_realloc,
	},
	{
		"pmemobj_tx_zrealloc",
		"ptzre",
		"<obj> <size> <type_num>",
		pocli_pmemobj_tx_zrealloc,
	},
	{
		"pmemobj_tx_strdup",
		"ptsd",
		"<obj> <string> <type_num>",
		pocli_pmemobj_tx_strdup,
	},
	{
		"pmemobj_tx_free",
		"ptf",
		"<obj>",
		pocli_pmemobj_tx_free,
	},
	{
		"pmemobj_tx_errno",
		"pter",
		"",
		pocli_pmemobj_tx_errno,
	},
	{
		"str_root_copy",
		"srcp",
		"<size> <string>",
		pocli_str_root_copy,
	},
	{
		"str_root_print",
		"srpr",
		"<size> <size>",
		pocli_str_root_print,
	}
};

#define POCLI_NCOMMANDS	(sizeof(pocli_commands) / sizeof(pocli_commands[0]))

/*
 * pocli_evn_parse_bool -- parse environment variable as boolean (1/0)
 */
static int
pocli_env_parse_bool(const char *envname, bool *value)
{
	char *env = os_getenv(envname);
	if (!env)
		return 0;
	if (strlen(env) > 1 || (env[0] != '0' && env[0] != '1')) {
		fprintf(stderr, "invalid value specified for %s -- '%s'\n",
				envname, env);
		return -1;
	}

	*value = env[0] != '0';

	return 0;
}

/*
 * pocli_read_opts -- read options from env variables
 */
static int
pocli_read_opts(struct pocli_opts *opts)
{
	/* default values */
	opts->exit_on_error = false;
	opts->echo_mode = false;
	opts->enable_comments = true;
	opts->enable_empty_cmds = true;
	opts->enable_long_names = true;
	opts->enable_help = true;

	int ret;

	ret = pocli_env_parse_bool(POCLI_ENV_EXIT_ON_ERROR,
			&opts->exit_on_error);
	if (ret)
		return ret;

	ret = pocli_env_parse_bool(POCLI_ENV_ECHO_MODE,
			&opts->echo_mode);
	if (ret)
		return ret;

	ret = pocli_env_parse_bool(POCLI_ENV_COMMENTS,
			&opts->enable_comments);
	if (ret)
		return ret;

	ret = pocli_env_parse_bool(POCLI_ENV_EMPTY_CMDS,
			&opts->enable_empty_cmds);
	if (ret)
		return ret;

	ret = pocli_env_parse_bool(POCLI_ENV_LONG_NAMES,
			&opts->enable_long_names);
	if (ret)
		return ret;

	ret = pocli_env_parse_bool(POCLI_ENV_HELP,
			&opts->enable_help);
	if (ret)
		return ret;

	return 0;
}

/*
 * pocli_alloc -- allocate main context
 */
static struct pocli *
pocli_alloc(FILE *input, const char *fname, const struct pocli_cmd *cmds,
		size_t ncmds, size_t inbuf_len, bool print_only)
{
	assert(inbuf_len < INT_MAX);
	struct pocli_opts opts;
	if (pocli_read_opts(&opts))
		return NULL;

	struct pocli *pcli = (struct pocli *)calloc(1, sizeof(*pcli));
	if (!pcli)
		return NULL;

	memcpy(&pcli->opts, &opts, sizeof(pcli->opts));
	pcli->in = input;
	pcli->istty = isatty(fileno(pcli->in));
	pcli->cmds = cmds;
	pcli->ncmds = ncmds;
	pcli->print_only = print_only;
	pcli->ctx.pocli = pcli;
	pcli->ctx.err = stderr;
	pcli->ctx.out = stdout;
	if (!print_only) {
		pcli->ctx.pop = pmemobj_open(fname, NULL);
		if (!pcli->ctx.pop) {
			fprintf(stderr, "%s: %s\n", fname, pmemobj_errormsg());
			goto err_free_pcli;
		}

		size_t root_size = pmemobj_root_size(pcli->ctx.pop);
		if (root_size)
			pcli->ctx.root = pmemobj_root(pcli->ctx.pop, root_size);
	}

	pcli->inbuf_len = inbuf_len;
	pcli->inbuf = (char *)malloc(inbuf_len);
	if (!pcli->inbuf)
		goto err_close_pool;

	return pcli;
err_close_pool:
	pmemobj_close(pcli->ctx.pop);
err_free_pcli:
	free(pcli);
	return NULL;
}

/*
 * pocli_free -- free main context
 */
static void
pocli_free(struct pocli *pcli)
{
	while (pmemobj_tx_stage() != TX_STAGE_NONE) {
		while (pmemobj_tx_stage() != TX_STAGE_NONE)
			pmemobj_tx_process();
		pmemobj_tx_end();
	}
	VEC_DELETE(&pcli->ctx.free_on_abort);
	if (pcli->ctx.pop)
		pmemobj_close(pcli->ctx.pop);

	free(pcli->inbuf);
	free(pcli);
}

/*
 * pocli_prompt -- print prompt
 */
static void
pocli_prompt(struct pocli *pcli)
{
	if (pcli->istty)
		printf(POCLI_CMD_PROMPT);
}

/*
 * pocli_process -- process input commands
 */
int
pocli_process(struct pocli *pcli)
{
	while (1) {
		pocli_prompt(pcli);

		if (!fgets(pcli->inbuf, (int)pcli->inbuf_len, pcli->in))
			return 0;

		char *nl = strchr(pcli->inbuf, '\n');
		if (!nl)
			return 1;
		*nl = '\0';
		char *hash = strchr(pcli->inbuf, '#');
		if (hash) {
			if (pcli->opts.enable_comments)
				*hash = '\0';
			else
				return 1;
		}

		if (pcli->inbuf[0] == 0 || pcli->inbuf[0] == '\n') {
			if (pcli->opts.enable_empty_cmds)
				continue;
			else
				return 1;
		}

		if (pcli->opts.echo_mode)
			pocli_printf(&pcli->ctx, "%s\n", pcli->inbuf);

		char *argstr = strchr(pcli->inbuf, ' ');
		if (argstr) {
			*argstr = '\0';
			argstr++;
		}
		char *cmds = pcli->inbuf;
		const struct pocli_cmd *cmd;
		if (pcli->print_only)
			cmd = &print_cmd;
		else
			cmd = pocli_get_cmd(pcli, cmds);

		if (!cmd) {
			pocli_err(&pcli->ctx, POCLI_RET_OK, /* XXX */
				"unknown command -- '%s'\n", cmds);
			if (pcli->opts.exit_on_error)
				return 1;
			else
				continue;
		}
		if (!argstr)
			argstr = cmds + strlen(pcli->inbuf) + 1;

		struct pocli_args *args = pocli_args_alloc(pcli->inbuf,
				argstr, POCLI_CMD_DELIM);
		if (!args)
			return 1;

		/*
		 * Put the args object on the stack, just in case we are
		 * in transaction, cmd->func will abort it and skip free(args).
		 */
		VEC_PUSH_BACK(&pcli->ctx.free_on_abort, args);

		enum pocli_ret ret = cmd->func(&pcli->ctx, args);
		free(args);

		/* Take args off the stack. */
		VEC_POP_BACK(&pcli->ctx.free_on_abort);

		if (ret != POCLI_RET_OK)
			return (int)ret;

	}
}

/*
 * pocli_do_process -- process input commands and return value
 */
static int
pocli_do_process(struct pocli *pcli)
{
	enum pocli_ret ret = (enum pocli_ret)pocli_process(pcli);

	if (ret == POCLI_RET_QUIT || ret == POCLI_RET_OK)
		return 0;
	else
		return 1;
}

/*
 * print_usage -- print usage of program
 */
static void
print_usage(const char *name)
{
	printf("Usage: %s [-s <script>] -p|<file>\n", name);
}

int
main(int argc, char *argv[])
{
	int opt;
	int ret = 1;
	const char *fname = NULL;
	FILE *input = stdin;
	bool print_only = false;

	while ((opt = getopt(argc, argv, "s:p")) != -1) {
		switch (opt) {
		case 's':
			input = os_fopen(optarg, "r");
			if (!input) {
				perror(optarg);
				goto out;
			}

			break;
		case 'p':
			print_only = true;
			break;
		default:
			print_usage(argv[0]);
			ret = 1;
			goto out;
		}
	}

	if (optind < argc) {
		fname = argv[optind];
	} else if (!print_only) {
		print_usage(argv[0]);
		goto out;
	}

	struct pocli *pcli = pocli_alloc(input, fname, pocli_commands,
			POCLI_NCOMMANDS, POCLI_INBUF_LEN, print_only);
	if (!pcli) {
		perror("pocli_alloc");
		goto out;
	}
	ret = pocli_do_process(pcli);

	pocli_free(pcli);
	fclose(input);

out:
	return ret;
}
