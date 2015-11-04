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

#include <libpmemobj.h>
#include "common.h"

#define	POCLI_ENV_EXIT_ON_ERROR	"PMEMOBJCLI_EXIT_ON_ERROR"
#define	POCLI_ENV_ECHO_MODE	"PMEMOBJCLI_ECHO_MODE"
#define	POCLI_ENV_COMMENTS	"PMEMOBJCLI_COMMENTS"
#define	POCLI_ENV_EMPTY_CMDS	"PMEMOBJCLI_EMPTY_CMDS"
#define	POCLI_ENV_LONG_NAMES	"PMEMOBJCLI_LONG_NAMES"
#define	POCLI_ENV_HELP		"PMEMOBJCLI_HELP"
#define	POCLI_CMD_DELIM	" "
#define	POCLI_CMD_PROMPT "pmemobjcli $ "
#define	POCLI_INBUF_LEN	4096
struct pocli;

/*
 * struct pocli_ctx -- pmemobjcli context structure for commands
 */
struct pocli_ctx {
	PMEMobjpool *pop;
	PMEMoid root;
	FILE *err;
	FILE *out;
	struct pocli *pocli;
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
};

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
 * pocli_args_type_num -- parse type number
 */
static enum pocli_ret
pocli_args_type_num(struct pocli_args *args, int arg, unsigned *type_num)
{
	assert(args != NULL);
	assert(arg >= 0 && arg < args->argc);
	assert(type_num != NULL);

	unsigned tn;
	char c;
	int ret = sscanf(args->argv[arg], "%u%c", &tn, &c);
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
	size_t size = sizeof (struct pocli_args);
	struct pocli_args *args = NULL;
	if (cmdstr) {
		size += sizeof (char *);
		args = malloc(size);
		if (!args)
			return NULL;
		args->argc = 1;
		args->argv[0] = cmdstr;
	}

	char *n = strtok(argstr, delim);
	while (n) {
		int cur = args ? args->argc++ : 0;
		size += sizeof (char *);
		struct pocli_args *nargs = realloc(args, size);
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

	if (!oidp)
		return POCLI_ERR_PARS;

	struct pocli_args *args = pocli_args_alloc(NULL, input, ".");
	if (!args)
		return POCLI_ERR_PARS;
	enum pocli_ret ret = POCLI_RET_OK;

	if (strcmp(args->argv[0], "r") != 0) {
		ret = POCLI_ERR_PARS;
		goto out;
	}

	PMEMoid *oid = &ctx->root;
	size_t size = pmemobj_root_size(ctx->pop);
	for (int i = 1; i < args->argc; i++) {

		unsigned ind;
		char c;
		int n = sscanf(args->argv[i], "%u%c", &ind, &c);
		if (n != 1) {
			ret = POCLI_ERR_PARS;
			goto out;
		}

		size_t max_ind = size / sizeof (PMEMoid);
		if (!max_ind || ind >= max_ind) {
			ret = POCLI_ERR_PARS;
			goto out;
		}

		PMEMoid *oids = pmemobj_direct(*oid);
		oid = &oids[ind];
		size = pmemobj_alloc_usable_size(*oid);
	}

	*oidp = oid;

out:
	free(input);
	free(args);
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

	int type_num = pmemobj_type_num(*oidp);

	pocli_printf(ctx, "%s(%s): type num = %d\n",
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

	pocli_printf(ctx, "%s(%llu): off = 0x%jx uuid = 0x%jx\n",
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
 * pocli_pmemobj_zalloc -- pmemobj_zalloc() command
 */
static enum pocli_ret
pocli_pmemobj_zalloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	unsigned type_num = 0;
	size_t size = 0;
	enum pocli_ret ret;

	ret = pocli_args_obj(ctx, args, 1, &oidp);
	if (ret)
		return ret;

	if (oidp == &ctx->root)
		return pocli_err(ctx, POCLI_ERR_ARGS,
				"cannot allocate to root object\n");

	ret = pocli_args_type_num(args, 2, &type_num);
	if (ret)
		return ret;

	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;

	int r = pmemobj_zalloc(ctx->pop, oidp, size, type_num);

	pocli_printf(ctx, "%s(%s, %llu, %u): %d\n",
		args->argv[0], args->argv[1], size, type_num, r);

	return ret;
}

/*
 * pocli_pmemobj_zrealloc -- pmemobj_zrealloc() command
 */
static enum pocli_ret
pocli_pmemobj_zrealloc(struct pocli_ctx *ctx, struct pocli_args *args)
{
	if (args->argc != 4)
		return POCLI_ERR_ARGS;

	PMEMoid *oidp = NULL;
	unsigned type_num = 0;
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

	ret = pocli_args_type_num(args, 2, &type_num);
	if (ret)
		return ret;

	ret = pocli_args_size(args, 3, &size);
	if (ret)
		return ret;

	int r = pmemobj_zrealloc(ctx->pop, oidp, size, type_num);

	pocli_printf(ctx, "%s(%s, %llu, %u): %d\n",
		args->argv[0], args->argv[1], size, type_num, r);

	return ret;
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

	pmemobj_free(oidp);

	pocli_printf(ctx, "%s(%p): off = 0x%llx uuid = 0x%llx\n",
		args->argv[0], oidp, oidp->off, oidp->pool_uuid_lo);

	return ret;
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
		.name		= "help",
		.name_short	= "h",
		.func		= pocli_help,
		.usage		= "[<cmd>]",
	},
	{
		.name		= "quit",
		.name_short	= "q",
		.func		= pocli_quit,
		.usage		= "",
	},
	{
		.name		= "pmemobj_root",
		.name_short	= "pr",
		.func		= pocli_pmemobj_root,
		.usage		= "<size>",
	},
	{
		.name		= "pmemobj_root_size",
		.name_short	= "prs",
		.func		= pocli_pmemobj_root_size,
		.usage		= "",
	},
	{
		.name		= "pmemobj_direct",
		.name_short	= "pdr",
		.func		= pocli_pmemobj_direct,
		.usage		= "<obj>",
	},
	{
		.name		= "pmemobj_alloc_usable_size",
		.name_short	= "paus",
		.func		= pocli_pmemobj_alloc_usable_size,
		.usage		= "<obj>",
	},
	{
		.name		= "pmemobj_zalloc",
		.name_short	= "pza",
		.func		= pocli_pmemobj_zalloc,
		.usage		= "<obj> <type_num> <size>",
	},
	{
		.name		= "pmemobj_zrealloc",
		.name_short	= "pzre",
		.func		= pocli_pmemobj_zrealloc,
		.usage		= "<obj> <type_num> <size>",
	},
	{
		.name		= "pmemobj_free",
		.name_short	= "pf",
		.func		= pocli_pmemobj_free,
		.usage		= "<obj>",
	},
	{
		.name		= "pmemobj_type_num",
		.name_short	= "ptn",
		.func		= pocli_pmemobj_type_num,
		.usage		= "<obj>",
	},
};

#define	POCLI_NCOMMANDS	(sizeof (pocli_commands) / sizeof (pocli_commands[0]))

/*
 * pocli_evn_parse_bool -- parse environment variable as boolean (1/0)
 */
static int
pocli_env_parse_bool(const char *envname, bool *value)
{
	char *env = getenv(envname);
	if (!env)
		return 0;
	if (strlen(env) > 1 || (env[0] != '0' && env[0] != '1')) {
		fprintf(stderr, "invalid value specified for %s -- '%s'\n",
				envname, env);
		return -1;
	}

	*value = env[0] - '0';

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
		size_t ncmds, size_t inbuf_len)
{
	assert(inbuf_len < INT_MAX);
	struct pocli_opts opts;
	if (pocli_read_opts(&opts))
		return NULL;

	struct pocli *pcli = calloc(1, sizeof (*pcli));
	if (!pcli)
		return NULL;

	memcpy(&pcli->opts, &opts, sizeof (pcli->opts));
	pcli->in = input;
	pcli->istty = isatty(fileno(pcli->in));
	pcli->cmds = cmds;
	pcli->ncmds = ncmds;
	pcli->ctx.pocli = pcli;
	pcli->ctx.err = stderr;
	pcli->ctx.out = stdout;
	pcli->ctx.pop = pmemobj_open(fname, NULL);
	if (!pcli->ctx.pop) {
		fprintf(stderr, "%s: %s\n", fname, pmemobj_errormsg());
		goto err_free_pcli;
	}

	size_t root_size = pmemobj_root_size(pcli->ctx.pop);
	if (root_size)
		pcli->ctx.root = pmemobj_root(pcli->ctx.pop, root_size);

	pcli->inbuf_len = inbuf_len;
	pcli->inbuf = malloc(inbuf_len);
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
static int
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
		const struct pocli_cmd *cmd = pocli_get_cmd(pcli, cmds);
		if (!cmd) {
			pocli_err(&pcli->ctx, 0,
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

		enum pocli_ret ret = cmd->func(&pcli->ctx, args);

		free(args);

		if (ret == POCLI_RET_QUIT)
			return 0;
		if (ret != POCLI_RET_OK)
			return 1;
	}
}

int
main(int argc, char *argv[])
{
	const char *fname = NULL;
	FILE *input = stdin;
	if (argc < 2 || argc > 4) {
		printf("usage: %s [-s <script>] <file>\n", argv[0]);
		return 1;
	}

	int is_script = strcmp(argv[1], "-s") == 0;

	if (is_script) {
		if (argc != 4) {
			if (argc == 2) {
				printf("usage: %s -s <script> <file>\n",
						argv[0]);
				return 1;
			} else if (argc == 3) {
				printf("usage: %s -s <script> <file> "
					"or %s <file>\n", argv[0], argv[2]);
				return 1;
			}
		}
		fname = argv[3];
		input = fopen(argv[2], "r");
		if (!input) {
			perror(argv[2]);
			return 1;
		}
	} else {
		if (argc != 2) {
			printf("usage: %s <file>\n", argv[0]);
			return 1;
		}
		fname = argv[1];
	}

	struct pocli *pcli = pocli_alloc(input, fname,
			pocli_commands, POCLI_NCOMMANDS, POCLI_INBUF_LEN);
	if (!pcli) {
		perror("pocli_alloc");
		return 1;
	}

	int ret = pocli_process(pcli);

	pocli_free(pcli);

	fclose(input);
	return ret;
}
