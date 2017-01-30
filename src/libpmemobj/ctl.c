/*
 * Copyright 2016-2017, Intel Corporation
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
 * ctl.c -- implementation of the interface for examination and modification of
 *	the library's internal state
 */

#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libpmem.h"
#include "libpmemobj.h"

#include "util.h"
#include "out.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "heap_layout.h"
#include "list.h"
#include "cuckoo.h"
#include "ctree.h"
#include "obj.h"
#include "sync.h"
#include "valgrind_internal.h"
#include "ctl.h"
#include "memblock.h"
#include "heap.h"

#define CTL_MAX_ENTRIES 100

#define MAX_CONFIG_FILE_LEN (1 << 20) /* 1 megabyte */

#define CTL_STRING_QUERY_SEPARATOR ";"
#define CTL_NAME_VALUE_SEPARATOR "="
#define CTL_QUERY_NODE_SEPARATOR "."
#define CTL_VALUE_ARG_SEPARATOR ","

static int ctl_global_first_free = 0;
static struct ctl_node CTL_NODE(global)[CTL_MAX_ENTRIES];

/*
 * This is the top level node of the ctl tree structure. Each node can contain
 * children and leaf nodes.
 *
 * Internal nodes simply create a new path in the tree whereas child nodes are
 * the ones providing the read/write functionality by the means of callbacks.
 *
 * Each tree node must be NULL-terminated, CTL_NODE_END macro is provided for
 * convience.
 */
struct ctl {
	struct ctl_node root[CTL_MAX_ENTRIES];
	int first_free;
};

/*
 * String provider is the simplest, elementary, query provider. It can be used
 * directly to parse environment variables or in conjuction with other code to
 * provide more complex behavior. It is initialized with a string containing all
 * of the queries and tokenizes it into separate structures.
 */
struct ctl_string_provider {
	struct ctl_query_provider super;

	char *buf; /* stores the entire string that needs to be parsed */
	char *sptr; /* for internal use of strtok */
};

/*
 * File provider builts on top of the string provider to facilitate reading
 * query data from a user-provided file.
 */
struct ctl_file_provider {
	struct ctl_string_provider super;
	FILE *config;
};

/*
 * ctl_find_node -- (internal) searches for a matching entry point in the
 *	provided nodes
 *
 * The caller is responsible for freeing all of the allocated indexes,
 * regardless of the return value.
 */
static struct ctl_node *
ctl_find_node(struct ctl_node *nodes, const char *name,
	struct ctl_indexes *indexes)
{
	struct ctl_node *n = NULL;
	char *sptr = NULL;
	char *parse_str = Strdup(name);
	if (parse_str == NULL)
		return NULL;

	char *node_name = strtok_r(parse_str, CTL_QUERY_NODE_SEPARATOR, &sptr);

	/*
	 * Go through the string and separate tokens that correspond to nodes
	 * in the main ctl tree.
	 */
	while (node_name != NULL) {
		char *endptr;
		long index_value = strtol(node_name, &endptr, 0);
		struct ctl_index *index_entry = NULL;
		if (endptr != node_name) { /* a valid index */
			index_entry = Malloc(sizeof(*index_entry));
			if (index_entry == NULL)
				goto error;
			index_entry->value = index_value;
			SLIST_INSERT_HEAD(indexes, index_entry, entry);
		}

		for (n = &nodes[0]; n->name != NULL; ++n) {
			if (index_entry && n->type == CTL_NODE_INDEXED)
				break;
			else if (strcmp(n->name, node_name) == 0)
				break;
		}
		if (n->name == NULL)
			goto error;

		if (index_entry)
			index_entry->name = n->name;

		nodes = n->children;
		node_name = strtok_r(NULL, CTL_QUERY_NODE_SEPARATOR, &sptr);
	}

	Free(parse_str);
	return n;

error:
	Free(parse_str);
	return NULL;
}

/*
 * ctl_delete_indexes --
 *	(internal) removes and frees all entires on the index list
 */
static void
ctl_delete_indexes(struct ctl_indexes *indexes)
{
	while (!SLIST_EMPTY(indexes)) {
		struct ctl_index *index = SLIST_FIRST(indexes);
		SLIST_REMOVE_HEAD(indexes, entry);
		Free(index);
	}
}

/*
 * ctl_parse_args -- (internal) parses a string argument based on the node
 *	structure
 */
static void *
ctl_parse_args(struct ctl_argument *arg_proto, char *arg)
{
	char *dest_arg = Malloc(arg_proto->dest_size);
	if (dest_arg == NULL)
		return NULL;

	char *sptr = NULL;
	char *arg_sep = strtok_r(arg, CTL_VALUE_ARG_SEPARATOR, &sptr);
	for (struct ctl_argument_parser *p = arg_proto->parsers;
		p->parser != NULL; ++p) {
		ASSERT(p->dest_offset + p->dest_size <= arg_proto->dest_size);
		if (arg_sep == NULL)
			goto error_parsing;

		if (p->parser(arg_sep, dest_arg + p->dest_offset,
			p->dest_size) != 0)
			goto error_parsing;

		arg_sep = strtok_r(NULL, CTL_VALUE_ARG_SEPARATOR, &sptr);
	}

	return dest_arg;

error_parsing:
	Free(dest_arg);
	return NULL;
}

/*
 * ctl_query_get_real_args -- (internal) returns a pointer with actual argument
 *	structure as required by the node callback
 */
static void *
ctl_query_get_real_args(struct ctl_node *n, void *write_arg,
	enum ctl_query_type type)
{
	void *real_arg = NULL;
	switch (type) {
		case CTL_QUERY_CONFIG_INPUT:
			real_arg = ctl_parse_args(n->arg, write_arg);
			break;
		case CTL_QUERY_PROGRAMMATIC:
			real_arg = write_arg;
			break;
		default:
			ASSERT(0);
			break;
	}

	return real_arg;
}

/*
 * ctl_query_cleanup_real_args -- (internal) cleanups relevant argument
 *	structures allocated as a result of the get_real_args call
 */
static void
ctl_query_cleanup_real_args(struct ctl_node *n, void *real_arg,
	enum ctl_query_type type)
{
	switch (type) {
		case CTL_QUERY_CONFIG_INPUT:
			Free(real_arg);
			break;
		case CTL_QUERY_PROGRAMMATIC:
			break;
		default:
			ASSERT(0);
			break;
	}
}

/*
 * ctl_query -- (internal) parses the name and calls the appropriate methods
 *	from the ctl tree
 */
static int
ctl_query(PMEMobjpool *pop, enum ctl_query_type type,
	const char *name, void *read_arg, void *write_arg)
{
	if (name == NULL)
		return -1;

	/*
	 * All of the indexes are put on this list so that the handlers can
	 * easily retrieve the index values. The list is cleared once the ctl
	 * query has been handled.
	 */
	struct ctl_indexes indexes;
	SLIST_INIT(&indexes);

	int ret = -1;

	struct ctl_node *n = ctl_find_node(CTL_NODE(global),
		name, &indexes);

	if (n == NULL && pop) {
		ctl_delete_indexes(&indexes);
		n = ctl_find_node(pop->ctl->root, name, &indexes);
	}

	/*
	 * Discard invalid calls, this includes the ones that are mostly correct
	 * but include an extraneous arguments.
	 */
	if (n == NULL ||
		(read_arg != NULL && n->read_cb == NULL) ||
		(write_arg != NULL && n->write_cb == NULL) ||
		(write_arg == NULL && read_arg == NULL)) {
		errno = EINVAL;
		goto error_invalid_arguments;
	}

	ASSERTeq(n->type, CTL_NODE_LEAF);

	ret = 0;

	if (read_arg)
		ret = n->read_cb(pop, type, read_arg, &indexes);

	if (write_arg && ret == 0) {
		void *real_arg = ctl_query_get_real_args(n, write_arg, type);
		if (real_arg == NULL) {
			errno = EINVAL;
			goto error_invalid_arguments;
		}
		ret = n->write_cb(pop, type, real_arg, &indexes);
		ctl_query_cleanup_real_args(n, real_arg, type);
	}

error_invalid_arguments:
	ctl_delete_indexes(&indexes);

	return ret;
}

/*
 * pmemobj_ctl_get -- programmatically executes a read ctl query
 */
int
pmemobj_ctl_get(PMEMobjpool *pop, const char *name, void *arg)
{
	return ctl_query(pop, CTL_QUERY_PROGRAMMATIC,
		name, arg, NULL);
}

/*
 * pmemobj_ctl_set -- programmatically executes a write ctl query
 */
int
pmemobj_ctl_set(PMEMobjpool *pop, const char *name, void *arg)
{
	return ctl_query(pop, CTL_QUERY_PROGRAMMATIC,
		name, NULL, arg);
}

/*
 * ctl_register_module_node -- adds a new node to the CTL tree root.
 */
void
ctl_register_module_node(struct ctl *c, const char *name, struct ctl_node *n)
{
	struct ctl_node *nnode = c == NULL ?
		&CTL_NODE(global)[ctl_global_first_free++] :
		&c->root[c->first_free++];

	nnode->children = n;
	nnode->type = CTL_NODE_NAMED;
	nnode->name = name;
}

/*
 * ctl_exec_query_config -- (internal) executes a ctl query from a provider
 */
static int
ctl_exec_query_config(PMEMobjpool *pop, struct ctl_query_config *q)
{
	return ctl_query(pop, CTL_QUERY_CONFIG_INPUT, q->name, NULL, q->value);
}

/*
 * ctl_load_config -- executes the entire query collection from a provider
 */
int
ctl_load_config(PMEMobjpool *pop, struct ctl_query_provider *p)
{
	int r = 0;

	struct ctl_query_config q = {NULL, NULL};

	for (r = p->first(p, &q); r == 0; r = p->next(p, &q)) {
		if ((r = ctl_exec_query_config(pop, &q)) != 0)
			break;
	}

	/* the method 'next' from data provider returns 1 to indicate end */
	return r >= 0 ? 0 : -1;
}

/*
 * ctl_string_provider_parse_query -- (internal) splits an entire query string
 *	into name and value
 */
static int
ctl_string_provider_parse_query(char *qbuf, struct ctl_query_config *q)
{
	if (qbuf == NULL)
		return 1;

	char *sptr;
	q->name = strtok_r(qbuf, CTL_NAME_VALUE_SEPARATOR, &sptr);
	if (q->name == NULL)
		return -1;

	q->value = strtok_r(NULL, CTL_NAME_VALUE_SEPARATOR, &sptr);
	if (q->value == NULL)
		return -1;

	/* the value itself mustn't include CTL_NAME_VALUE_SEPARATOR */
	char *extra = strtok_r(NULL, CTL_NAME_VALUE_SEPARATOR, &sptr);
	if (extra != NULL)
		return -1;

	return 0;
}

/*
 * ctl_string_provider_first -- (internal) returns the first query from the
 *	provider's collection
 */
static int
ctl_string_provider_first(struct ctl_query_provider *p,
	struct ctl_query_config *q)
{
	struct ctl_string_provider *sp = (struct ctl_string_provider *)p;

	char *qbuf = strtok_r(sp->buf, CTL_STRING_QUERY_SEPARATOR, &sp->sptr);

	return ctl_string_provider_parse_query(qbuf, q);
}

/*
 * ctl_string_provider_first -- (internal) returns the next in sequence query
 *	from the provider's collection
 */
static int
ctl_string_provider_next(struct ctl_query_provider *p,
	struct ctl_query_config *q)
{
	struct ctl_string_provider *sp = (struct ctl_string_provider *)p;

	ASSERTne(sp->sptr, NULL);

	char *qbuf = strtok_r(NULL, CTL_STRING_QUERY_SEPARATOR, &sp->sptr);

	return ctl_string_provider_parse_query(qbuf, q);
}

/*
 * ctl_string_provider_new --
 *	creates and initializes a new string query provider
 */
struct ctl_query_provider *
ctl_string_provider_new(const char *buf)
{
	struct ctl_string_provider *sp =
		Malloc(sizeof(struct ctl_string_provider));
	if (sp == NULL)
		goto error_provider_alloc;

	sp->super.first = ctl_string_provider_first;
	sp->super.next = ctl_string_provider_next;
	sp->buf = Strdup(buf);
	if (sp->buf == NULL)
		goto error_buf_alloc;

	return &sp->super;

error_buf_alloc:
	Free(sp);
error_provider_alloc:
	return NULL;
}

/*
 * ctl_string_provider_delete -- cleanups and deallocates provider instance
 */
void
ctl_string_provider_delete(struct ctl_query_provider *p)
{
	struct ctl_string_provider *sp = (struct ctl_string_provider *)p;
	Free(sp->buf);
	Free(sp);
}

/*
 * ctl_file_provider_new --
 *	creates and initializes a new file query provider
 *
 * This function opens up the config file, allocates a buffer of size equal to
 * the size of the file, reads its content and sanitizes it for the string query
 * provider pipeline.
 */
struct ctl_query_provider *
ctl_file_provider_new(const char *file)
{
	struct ctl_file_provider *fp =
		Malloc(sizeof(struct ctl_file_provider));
	if (fp == NULL)
		goto error_provider_alloc;

	struct ctl_string_provider *sp = &fp->super;

	sp->super.first = ctl_string_provider_first;
	sp->super.next = ctl_string_provider_next;
	if ((fp->config = fopen(file, "r")) == NULL)
		goto error_file_open;

	int err;
	if ((err = fseek(fp->config, 0, SEEK_END)) != 0)
		goto error_file_parse;

	long fsize = ftell(fp->config);
	if (fsize == -1)
		goto error_file_parse;
	if (fsize > MAX_CONFIG_FILE_LEN) {
		ERR("Config file too large");
		goto error_file_parse;
	}

	if ((err = fseek(fp->config, 0, SEEK_SET)) != 0)
		goto error_file_parse;

	sp->buf = Zalloc((size_t)fsize + 1); /* +1 for NULL-termination */
	if (sp->buf == NULL)
		goto error_file_parse;

	size_t bufpos = 0;

	int c;
	int is_comment_section = 0;
	while ((c = fgetc(fp->config)) != EOF) {
		if (c == '#')
			is_comment_section = 1;
		else if (c == '\n')
			is_comment_section = 0;
		else if (!is_comment_section && !isspace(c))
			sp->buf[bufpos++] = (char)c;
	}

	(void) fclose(fp->config);

	return &sp->super;

error_file_parse:
	fclose(fp->config);
error_file_open:
	Free(fp);
error_provider_alloc:
	return NULL;
}

/*
 * ctl_file_provider_delete -- cleanups and deallocates provider instance
 */
void
ctl_file_provider_delete(struct ctl_query_provider *p)
{
	struct ctl_file_provider *fp = (struct ctl_file_provider *)p;
	Free(fp->super.buf);
	Free(fp);
}

/*
 * ctl_new -- allocates and initalizes ctl data structures
 */
struct ctl *
ctl_new(void)
{
	struct ctl *c = Zalloc(sizeof(struct ctl));
	c->first_free = 0;

	return c;
}

/*
 * ctl_delete -- deletes ctl
 */
void
ctl_delete(struct ctl *c)
{
	Free(c);
}

/*
 * ctl_parse_ll -- (internal) parses and returns a long long signed integer
 */
static long long
ctl_parse_ll(const char *str)
{
	char *endptr;
	int olderrno = errno;
	errno = 0;
	long long val = strtoll(str, &endptr, 0);
	if (endptr == str || errno != 0)
		return LLONG_MIN;
	errno = olderrno;

	return val;
}

/*
 * ctl_arg_boolean -- checks whether the provided argument contains
 *	either a 1 or y or Y.
 */
int
ctl_arg_boolean(const void *arg, void *dest, size_t dest_size)
{
	int *intp = dest;
	char in = ((char *)arg)[0];

	if (tolower(in) == 'y' || in == '1') {
		*intp = 1;
		return 0;
	} else if (tolower(in) == 'n' || in == '0') {
		*intp = 0;
		return 0;
	}

	return -1;
}

/*
 * ctl_arg_integer -- parses signed integer argument
 */
int
ctl_arg_integer(const void *arg, void *dest, size_t dest_size)
{
	long long val = ctl_parse_ll(arg);
	if (val == LLONG_MIN)
		return -1;

	switch (dest_size) {
		case sizeof(int):
			if (val > INT_MAX || val < INT_MIN)
				return -1;
			*(int *)dest = (int)val;
			break;
		case sizeof(long long):
			*(long long *)dest = val;
			break;
	}

	return 0;
}

/*
 * ctl_arg_string -- verifies length and copies a string argument into a zeroed
 *	buffer
 */
int
ctl_arg_string(const void *arg, void *dest, size_t dest_size)
{
	/* check if the incoming string is longer or equal to dest_size */
	if (strnlen(arg, dest_size) == dest_size)
		return -1;

	strncpy(dest, arg, dest_size);

	return 0;
}
