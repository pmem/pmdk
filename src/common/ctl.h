/*
 * Copyright 2016-2019, Intel Corporation
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
 *     * Neither the name of the copyright holder nor the names of its
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
 * ctl.h -- internal declaration of statistics and control related structures
 */

#ifndef PMDK_CTL_H
#define PMDK_CTL_H 1

#include "queue.h"
#include "errno.h"
#include "out.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ctl;

struct ctl_index {
	const char *name;
	long value;
	PMDK_SLIST_ENTRY(ctl_index) entry;
};

PMDK_SLIST_HEAD(ctl_indexes, ctl_index);

enum ctl_query_source {
	CTL_UNKNOWN_QUERY_SOURCE,
	/* query executed directly from the program */
	CTL_QUERY_PROGRAMMATIC,
	/* query executed from the config file */
	CTL_QUERY_CONFIG_INPUT,

	MAX_CTL_QUERY_SOURCE
};

enum ctl_query_type {
	CTL_QUERY_READ,
	CTL_QUERY_WRITE,
	CTL_QUERY_RUNNABLE,

	MAX_CTL_QUERY_TYPE
};

typedef int (*node_callback)(void *ctx, enum ctl_query_source type,
	void *arg, struct ctl_indexes *indexes);

enum ctl_node_type {
	CTL_NODE_UNKNOWN,
	CTL_NODE_NAMED,
	CTL_NODE_LEAF,
	CTL_NODE_INDEXED,

	MAX_CTL_NODE
};

typedef int (*ctl_arg_parser)(const void *arg, void *dest, size_t dest_size);

struct ctl_argument_parser {
	size_t dest_offset; /* offset of the field inside of the argument */
	size_t dest_size; /* size of the field inside of the argument */
	ctl_arg_parser parser;
};

struct ctl_argument {
	size_t dest_size; /* sizeof the entire argument */
	struct ctl_argument_parser parsers[]; /* array of 'fields' in arg */
};

#define sizeof_member(t, m) sizeof(((t *)0)->m)

#define CTL_ARG_PARSER(t, p)\
{0, sizeof(t), p}

#define CTL_ARG_PARSER_STRUCT(t, m, p)\
{offsetof(t, m), sizeof_member(t, m), p}

#define CTL_ARG_PARSER_END {0, 0, NULL}

/*
 * CTL Tree node structure, do not use directly. All the necessery functionality
 * is provided by the included macros.
 */
struct ctl_node {
	const char *name;
	enum ctl_node_type type;

	node_callback cb[MAX_CTL_QUERY_TYPE];
	const struct ctl_argument *arg;

	const struct ctl_node *children;
};

struct ctl *ctl_new(void);
void ctl_delete(struct ctl *stats);

int ctl_load_config_from_string(struct ctl *ctl, void *ctx,
	const char *cfg_string);
int ctl_load_config_from_file(struct ctl *ctl, void *ctx,
	const char *cfg_file);

/* Use through CTL_REGISTER_MODULE, never directly */
void ctl_register_module_node(struct ctl *c,
	const char *name, struct ctl_node *n);

int ctl_arg_boolean(const void *arg, void *dest, size_t dest_size);
#define CTL_ARG_BOOLEAN {sizeof(int),\
	{{0, sizeof(int), ctl_arg_boolean},\
	CTL_ARG_PARSER_END}};

int ctl_arg_integer(const void *arg, void *dest, size_t dest_size);
#define CTL_ARG_INT {sizeof(int),\
	{{0, sizeof(int), ctl_arg_integer},\
	CTL_ARG_PARSER_END}};

#define CTL_ARG_LONG_LONG {sizeof(long long),\
	{{0, sizeof(long long), ctl_arg_integer},\
	CTL_ARG_PARSER_END}};

int ctl_arg_string(const void *arg, void *dest, size_t dest_size);
#define CTL_ARG_STRING(len) {len,\
	{{0, len, ctl_arg_string},\
	CTL_ARG_PARSER_END}};

#define CTL_STR(name) #name

#define CTL_NODE_END {NULL, CTL_NODE_UNKNOWN, {NULL, NULL, NULL}, NULL, NULL}

#define CTL_NODE(name)\
ctl_node_##name

int ctl_query(struct ctl *ctl, void *ctx, enum ctl_query_source source,
		const char *name, enum ctl_query_type type, void *arg);

/* Declaration of a new child node */
#define CTL_CHILD(name)\
{CTL_STR(name), CTL_NODE_NAMED, {NULL, NULL, NULL}, NULL,\
	(struct ctl_node *)CTL_NODE(name)}

/* Declaration of a new indexed node */
#define CTL_INDEXED(name)\
{CTL_STR(name), CTL_NODE_INDEXED, {NULL, NULL, NULL}, NULL,\
	(struct ctl_node *)CTL_NODE(name)}

#define CTL_READ_HANDLER(name, ...)\
ctl_##__VA_ARGS__##_##name##_read

#define CTL_WRITE_HANDLER(name, ...)\
ctl_##__VA_ARGS__##_##name##_write

#define CTL_RUNNABLE_HANDLER(name, ...)\
ctl_##__VA_ARGS__##_##name##_runnable

#define CTL_ARG(name)\
ctl_arg_##name

/*
 * Declaration of a new read-only leaf. If used the corresponding read function
 * must be declared by CTL_READ_HANDLER macro.
 */
#define CTL_LEAF_RO(name, ...)\
{CTL_STR(name), CTL_NODE_LEAF, \
	{CTL_READ_HANDLER(name, __VA_ARGS__), NULL, NULL}, NULL, NULL}

/*
 * Declaration of a new write-only leaf. If used the corresponding write
 * function must be declared by CTL_WRITE_HANDLER macro.
 */
#define CTL_LEAF_WO(name, ...)\
{CTL_STR(name), CTL_NODE_LEAF, \
	{NULL, CTL_WRITE_HANDLER(name, __VA_ARGS__), NULL},\
	&CTL_ARG(name), NULL}

/*
 * Declaration of a new runnable leaf. If used the corresponding run
 * function must be declared by CTL_RUNNABLE_HANDLER macro.
 */
#define CTL_LEAF_RUNNABLE(name, ...)\
{CTL_STR(name), CTL_NODE_LEAF, \
	{NULL, NULL, CTL_RUNNABLE_HANDLER(name, __VA_ARGS__)},\
	NULL, NULL}

/*
 * Declaration of a new read-write leaf. If used both read and write function
 * must be declared by CTL_READ_HANDLER and CTL_WRITE_HANDLER macros.
 */
#define CTL_LEAF_RW(name)\
{CTL_STR(name), CTL_NODE_LEAF,\
	{CTL_READ_HANDLER(name), CTL_WRITE_HANDLER(name), NULL},\
	&CTL_ARG(name), NULL}

#define CTL_REGISTER_MODULE(_ctl, name)\
ctl_register_module_node((_ctl), CTL_STR(name),\
(struct ctl_node *)CTL_NODE(name))

#ifdef __cplusplus
}
#endif

#endif
