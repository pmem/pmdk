/*
 * Copyright 2017, Intel Corporation
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
 * badblock_poolset.c - implementation of poolset badblock source
 *
 * Poolset badblock iterator is a composition of lower-level badblock sources
 * such as file or devdax plugins.
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <queue.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "set.h"
#include "sysfs.h"
#include "extent.h"
#include "plugin.h"
#include "badblock.h"
#include "badblock_poolset.h"

struct badblock_part {
	struct badblock_iter *iter;

	SLIST_ENTRY(badblock_part) e;

	size_t nbadblocks;
	size_t pos;
	struct badblock badblocks[];
};

struct badblock_iter_poolset {
	struct {
		int (*next)(struct badblock_iter_poolset *iter,
			struct badblock *b);
		int (*clear)(struct badblock_iter_poolset *iter,
			struct badblock *b);
		size_t (*count)(struct badblock_iter_poolset *iter);
		void (*del)(struct badblock_iter_poolset *iter);
	} i_ops;

	size_t nbadblocks;

	struct badblock_part *current;
	SLIST_HEAD(, badblock_part) parts;
};

/*
 * badblock_next -- returns the current badblock and bumps the iterator position
 */
static int
badblock_next(struct badblock_iter_poolset *iter, struct badblock *badblock)
{
	struct badblock_part *part = iter->current;

	if (part == NULL)
		return -1;

	if (part->nbadblocks == part->pos) {
		iter->current = SLIST_NEXT(part, e);
		return badblock_next(iter, badblock);
	}

	*badblock = part->badblocks[part->pos++];

	return 0;
}

/*
 * badblock_del -- deletes the badblock iterator instance
 */
static void
badblock_del(struct badblock_iter_poolset *iter)
{
	while (!SLIST_EMPTY(&iter->parts)) {
		struct badblock_part *part = SLIST_FIRST(&iter->parts);
		part->iter->i_ops.del(part->iter);
		Free(part);
		SLIST_REMOVE_HEAD(&iter->parts, e);
	}
	Free(iter);
}

/*
 * badblock_count -- length of the badblock iterator
 */
static size_t
badblock_count(struct badblock_iter_poolset *iter)
{
	return iter->nbadblocks;
}

/*
 * badblock_clear -- clear a badblock
 */
static int
badblock_clear(struct badblock_iter_poolset *iter, struct badblock *b)
{
	LOG(3, "length %" PRIu64 " offset %" PRIu64, b->length, b->offset);

	struct badblock_part *part;
	SLIST_FOREACH(part, &iter->parts, e) {
		for (size_t i = 0; i < part->nbadblocks; ++i) {
			if (part->badblocks[i].offset == b->offset)
				return part->iter->i_ops.clear(part->iter, b);
		}
	}

	return -1;
}

/*
 * part_cb -- registers new part subiterator
 */
static int
part_cb(struct part_file *pf, void *arg)
{
	if (pf->is_remote) /* not supported yet */
		return 0;

	struct badblock_iter_poolset *poolset_iter = arg;

	struct badblock_iter *iter = badblock_new(pf->path);
	if (iter == NULL)
		return 1;

	struct badblock_part *part = Malloc(sizeof(*part) +
		sizeof(struct badblock) * iter->i_ops.count(iter));
	if (part == NULL)
		goto error_part_alloc;

	part->iter = iter;
	part->nbadblocks = 0;
	part->pos = 0;
	while (iter->i_ops.next(iter,
		&part->badblocks[part->nbadblocks]) == 0)
		part->nbadblocks++;

	poolset_iter->nbadblocks += part->nbadblocks;

	SLIST_INSERT_HEAD(&poolset_iter->parts, part, e);

	return 0;

error_part_alloc:
	iter->i_ops.del(iter);
	return 1;
}

/*
 * iter_from_file -- creates a new badblock iterator
 */
static struct badblock_iter_poolset *
iter_from_file(const char *file)
{
	LOG(3, "%s", file);

	if (!util_is_poolset_file(file))
		return NULL;

	struct badblock_iter_poolset *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		goto error_iter_alloc;

	iter->nbadblocks = 0;
	SLIST_INIT(&iter->parts);

	if (util_poolset_foreach_part(file, part_cb, iter) != 0)
		goto error_foreach_part;

	iter->current = SLIST_FIRST(&iter->parts);

	iter->i_ops.next = badblock_next;
	iter->i_ops.clear = badblock_clear;
	iter->i_ops.del = badblock_del;
	iter->i_ops.count = badblock_count;

	return iter;

error_iter_alloc:
error_foreach_part:
	badblock_del(iter);
	return NULL;
}

/*
 * pmem_plugin_desc -- linux file badblock descriptor
 */
static void
pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs)
{
	*module_name = "badblock_source";
	*name = "builtin_poolset_source";
	*version = 1;
	*funcs = iter_from_file;
}

/*
 * pmem_plugin_load -- noop
 */
static int
pmem_plugin_load(void)
{
	return 0;
}

/*
 * pmem_plugin_unload -- noop
 */
static void
pmem_plugin_unload(void)
{

}

/*
 * poolset badblock plugin
 */
static struct plugin_ops badblock_poolset_source_plugin = {
	pmem_plugin_desc, pmem_plugin_load, pmem_plugin_unload,
};

/*
 * badblock_poolset_source_add -- registers the poolset badblock source
 */
void
badblock_poolset_source_add(void)
{
	plugin_add(&badblock_poolset_source_plugin);
}
