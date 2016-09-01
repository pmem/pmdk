/*
 * Copyright 2014-2016, Intel Corporation
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
 * ctl.c -- implementation of the interface for examining and modification of
 *	the library internal state
 */

#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

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

static int
CTL_READ_HANDLER(test_rw)(PMEMobjpool *pop, void *arg,
	struct ctl_indexes *indexes)
{
	int *arg_rw = arg;
	*arg_rw = 0;

	return 0;
}

static int
CTL_WRITE_HANDLER(test_rw)(PMEMobjpool *pop, void *arg,
	struct ctl_indexes *indexes)
{
	int *arg_rw = arg;
	*arg_rw = 1;

	return 0;
}

static int
CTL_WRITE_HANDLER(test_wo)(PMEMobjpool *pop, void *arg,
	struct ctl_indexes *indexes)
{
	int *arg_wo = arg;
	*arg_wo = 1;

	return 0;
}

static int
CTL_READ_HANDLER(test_ro)(PMEMobjpool *pop, void *arg,
	struct ctl_indexes *indexes)
{
	int *arg_ro = arg;
	*arg_ro = 0;

	return 0;
}

static int
CTL_READ_HANDLER(index_value)(PMEMobjpool *pop, void *arg,
	struct ctl_indexes *indexes)
{
	long *index_value = arg;
	struct ctl_index *idx = SLIST_FIRST(indexes);
	ASSERT(strcmp(idx->name, "test_index") == 0);
	*index_value = idx->value;

	return 0;
}

static const struct ctl_node CTL_NODE(test_index)[] = {
	CTL_LEAF_RO(index_value),
	CTL_NODE_END
};

static const struct ctl_node CTL_NODE(debug)[] = {
	CTL_LEAF_RO(test_ro),
	CTL_LEAF_WO(test_wo),
	CTL_LEAF_RW(test_rw),
	CTL_INDEXED(test_index),

	CTL_NODE_END
};

#define CTL_MAX_ENTRIES 100

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
 * pmemobj_ctl -- parses the name and calls the appropriate methods from the ctl
 *	tree.
 */
int
pmemobj_ctl(PMEMobjpool *pop, const char *name, void *read_arg, void *write_arg)
{
	struct ctl_node *nodes = pop->ctl->root;
	struct ctl_node *n = NULL;

	/*
	 * All of the indexes are put on this list so that the handlers can
	 * easily retrieve the index values. The list is cleared once the ctl
	 * query has been handled.
	 */
	struct ctl_indexes indexes;
	SLIST_INIT(&indexes);

	int ret = -1;

	char *parse_str = Strdup(name);
	if (parse_str == NULL)
		goto error_strdup_name;

	char *sptr = NULL;
	char *node_name = strtok_r(parse_str, ".", &sptr);

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
				goto error_index_malloc;
			index_entry->value = index_value;
		}

		for (n = &nodes[0]; n->name != NULL; ++n) {
			if (index_entry && n->type == CTL_NODE_INDEXED)
				break;
			else if (strcmp(n->name, node_name) == 0)
				break;
		}
		if (n->name == NULL) {
			errno = EINVAL;
			goto error_invalid_arguments;
		}
		nodes = n->children;
		node_name = strtok_r(NULL, ".", &sptr);
		if (index_entry) {
			index_entry->name = n->name;
			SLIST_INSERT_HEAD(&indexes, index_entry, entry);
		}
	}

	/*
	 * Discard invalid calls, this includes the ones that are mostly correct
	 * but include an extraneous arguments.
	 */
	if (n == NULL || (read_arg != NULL && n->read_cb == NULL) ||
		(write_arg != NULL && n->write_cb == NULL) ||
		(write_arg == NULL && read_arg == NULL)) {
		errno = EINVAL;
		goto error_invalid_arguments;
	}

	ASSERTeq(n->type, CTL_NODE_LEAF);

	ret = 0;

	if (read_arg)
		ret = n->read_cb(pop, read_arg, &indexes);

	if (write_arg && ret == 0)
		ret = n->write_cb(pop, write_arg, &indexes);

error_invalid_arguments:
	while (!SLIST_EMPTY(&indexes)) {
		struct ctl_index *index = SLIST_FIRST(&indexes);
		Free(index);
		SLIST_REMOVE_HEAD(&indexes, entry);
	}

error_index_malloc:
	Free(parse_str);

error_strdup_name:
	return ret;
}

/*
 * ctl_register_module_node -- adds a new node to the CTL tree root.
 */
void
ctl_register_module_node(struct ctl *c, const char *name, struct ctl_node *n)
{
	struct ctl_node *nnode = &c->root[c->first_free++];
	nnode->children = n;
	nnode->type = CTL_NODE_NAMED;
	nnode->name = Strdup(name);
}

/*
 * ctl_new -- allocates and initalizes ctl data structures
 */
struct ctl *
ctl_new(void)
{
	struct ctl *c = Zalloc(sizeof(struct ctl));
	c->first_free = 0;
	CTL_REGISTER_MODULE(c, debug);

	return c;
}

/*
 * ctl_delete -- deletes statistics
 */
void
ctl_delete(struct ctl *c)
{
	for (struct ctl_node *n = c->root; n->name != NULL; ++n)
		Free(n->name);

	Free(c);
}
