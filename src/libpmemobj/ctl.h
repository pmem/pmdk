/*
 * Copyright 2016, Intel Corporation
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

#ifndef LIBPMEMOBJ_CTL_H
#define LIBPMEMOBJ_CTL_H 1

#include "libpmemobj.h"
#include <sys/queue.h>

struct ctl;

struct ctl_index {
	char *name;
	long value;
	SLIST_ENTRY(ctl_index) entry;
};

SLIST_HEAD(ctl_indexes, ctl_index);

enum ctl_query_type {
	CTL_UNKNOWN_QUERY_TYPE,
	CTL_QUERY_PROGRAMMATIC,
	CTL_QUERY_CONFIG_INPUT,

	MAX_CTL_QUERY_TYPE
};

typedef int (*node_callback)(PMEMobjpool *pop, enum ctl_query_type type,
	void *arg, struct ctl_indexes *indexes);

enum ctl_node_type {
	CTL_NODE_UNKNOWN,
	CTL_NODE_NAMED,
	CTL_NODE_LEAF,
	CTL_NODE_INDEXED,

	MAX_CTL_NODE
};

/*
 * CTL Tree node structure, do not use directly. All the necessery functionality
 * is provided by the included macros.
 */
struct ctl_node {
	char *name;
	enum ctl_node_type type;

	node_callback read_cb;
	node_callback write_cb;

	struct ctl_node *children;
};

struct ctl_query_config {
	char *name;
	char *value;
};

struct ctl_query_provider {
	/*
	 * Both functions return:
	 *  0 if the query variable has been successfully populated with data.
	 *  1 if the iteration reached the end of the collection.
	 * -1 if a parsing error occured.
	 */
	int (*first)(struct ctl_query_provider *p, struct ctl_query_config *q);
	int (*next)(struct ctl_query_provider *p, struct ctl_query_config *q);
};

struct ctl_query_provider *ctl_string_provider_new(const char *buf);
void ctl_string_provider_delete(struct ctl_query_provider *p);

struct ctl_query_provider *ctl_file_provider_new(const char *file);
void ctl_file_provider_delete(struct ctl_query_provider *p);

struct ctl *ctl_new(void);
int ctl_load_config(PMEMobjpool *pop, struct ctl_query_provider *p);
void ctl_delete(struct ctl *stats);

/* Use through CTL_REGISTER_MODULE, never directly */
void ctl_register_module_node(struct ctl *c,
	const char *name, struct ctl_node *n);

#define CTL_STR(name) #name

#define CTL_NODE_END {NULL, CTL_NODE_UNKNOWN, NULL, NULL, NULL}

#define CTL_NODE(name)\
ctl_node_##name

/* Declaration of a new child node */
#define CTL_CHILD(name)\
{CTL_STR(name), CTL_NODE_NAMED, NULL, NULL, (struct ctl_node *)CTL_NODE(name)}

/* Declaration of a new indexed node */
#define CTL_INDEXED(name)\
{CTL_STR(name), CTL_NODE_INDEXED, NULL, NULL, (struct ctl_node *)CTL_NODE(name)}

#define CTL_READ_HANDLER(name)\
ctl_##name##_read

#define CTL_WRITE_HANDLER(name)\
ctl_##name##_write

/*
 * Declaration of a new read-only leaf. If used the corresponding read function
 * must be declared by CTL_READ_HANDLER or CTL_GEN_RO_STAT macros.
 */
#define CTL_LEAF_RO(name)\
{CTL_STR(name), CTL_NODE_LEAF, CTL_READ_HANDLER(name), NULL, NULL}

/*
 * Declaration of a new write-only leaf. If used the corresponding write
 * function must be declared by CTL_WRITE_HANDLER macro.
 */
#define CTL_LEAF_WO(name)\
{CTL_STR(name), CTL_NODE_LEAF, NULL, CTL_WRITE_HANDLER(name), NULL}

/*
 * Declaration of a new read-write leaf. If used both read and write function
 * must be declared by CTL_READ_HANDLER and CTL_WRITE_HANDLER macros.
 */
#define CTL_LEAF_RW(name)\
{CTL_STR(name), CTL_NODE_LEAF,\
	CTL_READ_HANDLER(name), CTL_WRITE_HANDLER(name), NULL}

#define CTL_REGISTER_MODULE(_ctl, name)\
ctl_register_module_node((_ctl), CTL_STR(name),\
(struct ctl_node *)CTL_NODE(name))

#endif
