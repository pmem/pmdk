/*
 * Copyright 2018-2019, Intel Corporation
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
 * container_ravl.c -- implementation of ravl-based block container
 */

#include "container_ravl.h"
#include "ravl.h"
#include "out.h"
#include "sys_util.h"

struct block_container_ravl {
	struct block_container super;
	struct ravl *tree;
};

/*
 * container_compare_memblocks -- (internal) compares two memory blocks
 */
static int
container_compare_memblocks(const void *lhs, const void *rhs)
{
	const struct memory_block *l = lhs;
	const struct memory_block *r = rhs;

	int64_t diff = (int64_t)l->size_idx - (int64_t)r->size_idx;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->zone_id - (int64_t)r->zone_id;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->chunk_id - (int64_t)r->chunk_id;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	diff = (int64_t)l->block_off - (int64_t)r->block_off;
	if (diff != 0)
		return diff > 0 ? 1 : -1;

	return 0;
}

/*
 * container_ravl_insert_block -- (internal) inserts a new memory block
 *	into the container
 */
static int
container_ravl_insert_block(struct block_container *bc,
	const struct memory_block *m)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	struct memory_block *e = m->m_ops->get_user_data(m);
	VALGRIND_DO_MAKE_MEM_DEFINED(e, sizeof(*e));
	VALGRIND_ADD_TO_TX(e, sizeof(*e));
	*e = *m;
	VALGRIND_SET_CLEAN(e, sizeof(*e));
	VALGRIND_REMOVE_FROM_TX(e, sizeof(*e));

	return ravl_insert(c->tree, e);
}

/*
 * container_ravl_get_rm_block_bestfit -- (internal) removes and returns the
 *	best-fit memory block for size
 */
static int
container_ravl_get_rm_block_bestfit(struct block_container *bc,
	struct memory_block *m)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	struct ravl_node *n = ravl_find(c->tree, m,
		RAVL_PREDICATE_GREATER_EQUAL);
	if (n == NULL)
		return ENOMEM;

	struct memory_block *e = ravl_data(n);
	*m = *e;
	ravl_remove(c->tree, n);

	return 0;
}

/*
 * container_ravl_get_rm_block_exact --
 *	(internal) removes exact match memory block
 */
static int
container_ravl_get_rm_block_exact(struct block_container *bc,
	const struct memory_block *m)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	struct ravl_node *n = ravl_find(c->tree, m, RAVL_PREDICATE_EQUAL);
	if (n == NULL)
		return ENOMEM;

	ravl_remove(c->tree, n);

	return 0;
}

/*
 * container_ravl_is_empty -- (internal) checks whether the container is empty
 */
static int
container_ravl_is_empty(struct block_container *bc)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	return ravl_empty(c->tree);
}

/*
 * container_ravl_rm_all -- (internal) removes all elements from the tree
 */
static void
container_ravl_rm_all(struct block_container *bc)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	ravl_clear(c->tree);
}

/*
 * container_ravl_delete -- (internal) deletes the container
 */
static void
container_ravl_destroy(struct block_container *bc)
{
	struct block_container_ravl *c =
		(struct block_container_ravl *)bc;

	ravl_delete(c->tree);

	Free(bc);
}

/*
 * Tree-based block container used to provide best-fit functionality to the
 * bucket. The time complexity for this particular container is O(k) where k is
 * the length of the key.
 *
 * The get methods also guarantee that the block with lowest possible address
 * that best matches the requirements is provided.
 */
static const struct block_container_ops container_ravl_ops = {
	.insert = container_ravl_insert_block,
	.get_rm_exact = container_ravl_get_rm_block_exact,
	.get_rm_bestfit = container_ravl_get_rm_block_bestfit,
	.is_empty = container_ravl_is_empty,
	.rm_all = container_ravl_rm_all,
	.destroy = container_ravl_destroy,
};

/*
 * container_new_ravl -- allocates and initializes a ravl container
 */
struct block_container *
container_new_ravl(struct palloc_heap *heap)
{
	struct block_container_ravl *bc = Malloc(sizeof(*bc));
	if (bc == NULL)
		goto error_container_malloc;

	bc->super.heap = heap;
	bc->super.c_ops = &container_ravl_ops;
	bc->tree = ravl_new(container_compare_memblocks);
	if (bc->tree == NULL)
		goto error_ravl_new;

	return (struct block_container *)&bc->super;

error_ravl_new:
	Free(bc);

error_container_malloc:
	return NULL;
}
