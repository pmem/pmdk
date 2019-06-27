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
 * critnib.c -- implementation of critnib tree
 *
 * It offers identity lookup (like a hashmap) and <= lookup (like a search
 * tree).  Unlike some hashing algorithms (cuckoo hash, perfect hashing) the
 * complexity isn't constant, but for data sizes we expect it's several
 * times as fast as cuckoo, and has no "stop the world" cases that would
 * cause latency (ie, better worst case behaviour).
 */


/*
 * STRUCTURE DESCRIPTION
 *
 * Critnib is a hybrid between a radix tree and DJ Bernstein's critbit:
 * it skips nodes for uninteresting radix nodes (ie, ones that would have
 * exactly one child), this requires adding to every node a field that
 * describes the slice (4-bit in our case) that this radix level is for.
 *
 * This implementation also stores each node's path (ie, bits that are
 * common to every key in that subtree) -- this doesn't help with lookups
 * at all (unused in == match, could be reconstructed at no cost in <=
 * after first dive) but simplifies inserts and removes.  If we ever want
 * that piece of memory it's easy to trim it down.
 */

/*
 * CONCURRENCY ISSUES
 *
 * Reads are completely lock-free sync-free, but only almost wait-free:
 * if for some reason a read thread gets pathologically stalled, it will
 * notice the data being stale and restart the work.  In usual cases,
 * the structure having been modified does _not_ cause a restart.
 *
 * Writes could be easily made lock-free as well (with only a cmpxchg
 * sync), but this leads to problems with removes.  A possible solution
 * would be doing removes by overwriting by NULL w/o freeing -- yet this
 * would lead to the structure growing without bounds.  Complex per-node
 * locks would increase concurrency but they slow down individual writes
 * enough that in practice a simple global write lock works faster.
 *
 * Removes are the only operation that can break reads.  The structure
 * can do local RCU well -- the problem being knowing when it's safe to
 * free.  Any synchronization with reads would kill their speed, thus
 * instead we have a remove count.  The grace period is DELETED_LIFE,
 * after which any read will notice staleness and restart its work.
 */
#include <errno.h>
#include <stdbool.h>

#include "alloc.h"
#include "critnib.h"
#include "out.h"
#include "sys_util.h"
#include "valgrind_internal.h"

/*
 * A node that has been deleted is left untouched for this many delete
 * cycles.  Reads have guaranteed correctness if they took no longer than
 * DELETED_LIFE concurrent deletes, otherwise they notice something is
 * wrong and restart.  The memory of deleted nodes is never freed to
 * malloc nor their pointers lead anywhere wrong, thus a stale read will
 * (temporarily) get a wrong answer but won't crash.
 *
 * There's no need to count writes as they never interfere with reads.
 *
 * Allowing stale reads (of arbitrarily old writes or of deletes less than
 * DELETED_LIFE old) might sound counterintuitive, but it doesn't affect
 * semantics in any way: the thread could have been stalled just after
 * returning from our code.  Thus, the guarantee is: the result of get() or
 * find_le() is a value that was current at any point between the call
 * start and end.
 */
#define DELETED_LIFE 16

#define SLICE 4
#define NIB ((1ULL << SLICE) - 1)
#define SLNODES (1 << SLICE)

typedef unsigned char sh_t;

struct critnib_node {
	/*
	 * path is the part of a tree that's already traversed (be it through
	 * explicit nodes or collapsed links) -- ie, any subtree below has all
	 * those bits set to this value.
	 *
	 * nib is a 4-bit slice that's an index into the node's children.
	 *
	 * shift is the length (in bits) of the part of the key below this node.
	 *
	 *            nib
	 * |XXXXXXXXXX|?|*****|
	 *    path      ^
	 *              +-----+
	 *               shift
	 */
	struct critnib_node *child[SLNODES];
	uint64_t path;
	sh_t shift;
};

struct critnib_leaf {
	uint64_t key;
	void *value;
};

struct critnib {
	struct critnib_node *root;

	/* pool of freed nodes: singly linked list, next at child[0] */
	struct critnib_node *deleted_node;
	struct critnib_leaf *deleted_leaf;

	/* nodes removed but not yet eligible for reuse */
	struct critnib_node *pending_del_nodes[DELETED_LIFE];
	struct critnib_leaf *pending_del_leaves[DELETED_LIFE];

	uint64_t remove_count;

	os_mutex_t mutex; /* writes/removes */
};

/*
 * atomic load
 */
static void
load(void *src, void *dst)
{
	util_atomic_load_explicit64((uint64_t *)src, (uint64_t *)dst,
		memory_order_acquire);
}

/*
 * atomic store
 */
static void
store(void *dst, void *src)
{
	util_atomic_store_explicit64((uint64_t *)dst, (uint64_t)src,
		memory_order_release);
}

/*
 * internal: is_leaf -- check tagged pointer for leafness
 */
static inline bool
is_leaf(struct critnib_node *n)
{
	return (uint64_t)n & 1;
}

/*
 * internal: to_leaf -- untag a leaf pointer
 */
static inline struct critnib_leaf *
to_leaf(struct critnib_node *n)
{
	return (void *)((uint64_t)n & ~1ULL);
}

/*
 * internal: path_mask -- return bit mask of a path above a subtree [shift]
 * bits tall
 */
static inline uint64_t
path_mask(sh_t shift)
{
	return ~NIB << shift;
}

/*
 * internal: slice_index -- return index of child at the given nib
 */
static inline unsigned
slice_index(uint64_t key, sh_t shift)
{
	return (key >> shift) & NIB;
}

/*
 * critnib_new -- allocates a new critnib structure
 */
struct critnib *
critnib_new(void)
{
	struct critnib *c = Zalloc(sizeof(struct critnib));
	if (!c)
		return NULL;

	util_mutex_init(&c->mutex);

	VALGRIND_HG_DRD_DISABLE_CHECKING(&c->root, sizeof(c->root));
	VALGRIND_HG_DRD_DISABLE_CHECKING(&c->remove_count,
					sizeof(c->remove_count));

	return c;
}

/*
 * internal: delete_node -- recursively free (to malloc) a subtree
 */
static void
delete_node(struct critnib_node *__restrict n)
{
	if (!is_leaf(n)) {
		for (int i = 0; i < SLNODES; i++) {
			if (n->child[i])
				delete_node(n->child[i]);
		}

		Free(n);
	} else {
		Free(to_leaf(n));
	}
}

/*
 * critnib_delete -- destroy and free a critnib struct
 */
void
critnib_delete(struct critnib *c)
{
	if (c->root)
		delete_node(c->root);

	util_mutex_destroy(&c->mutex);

	for (struct critnib_node *m = c->deleted_node; m; ) {
		struct critnib_node *mm = m->child[0];
		Free(m);
		m = mm;
	}

	for (struct critnib_leaf *k = c->deleted_leaf; k; ) {
		struct critnib_leaf *kk = k->value;
		Free(k);
		k = kk;
	}

	for (int i = 0; i < DELETED_LIFE; i++) {
		Free(c->pending_del_nodes[i]);
		Free(c->pending_del_leaves[i]);
	}

	Free(c);
}

/*
 * internal: free_node -- free (to internal pool, not malloc) a node.
 *
 * We cannot free them to malloc as a stalled reader thread may still walk
 * through such nodes; it will notice the result being bogus but only after
 * completing the walk, thus we need to ensure any freed nodes still point
 * to within the critnib structure.
 */
static void
free_node(struct critnib *__restrict c, struct critnib_node *__restrict n)
{
	if (!n)
		return;

	ASSERT(!is_leaf(n));
	n->child[0] = c->deleted_node;
	c->deleted_node = n;
}

/*
 * internal: alloc_node -- allocate a node from our pool or from malloc
 */
static struct critnib_node *
alloc_node(struct critnib *__restrict c)
{
	if (!c->deleted_node) {
		struct critnib_node *n = Malloc(sizeof(struct critnib_node));
		if (n == NULL)
			ERR("!Malloc");

		return n;
	}

	struct critnib_node *n = c->deleted_node;

	c->deleted_node = n->child[0];
	VALGRIND_ANNOTATE_NEW_MEMORY(n, sizeof(*n));

	return n;
}

/*
 * internal: free_leaf -- free (to internal pool, not malloc) a leaf.
 *
 * See free_node().
 */
static void
free_leaf(struct critnib *__restrict c, struct critnib_leaf *__restrict k)
{
	if (!k)
		return;

	k->value = c->deleted_leaf;
	c->deleted_leaf = k;
}

/*
 * internal: alloc_leaf -- allocate a leaf from our pool or from malloc
 */
static struct critnib_leaf *
alloc_leaf(struct critnib *__restrict c)
{
	if (!c->deleted_leaf) {
		struct critnib_leaf *k = Malloc(sizeof(struct critnib_leaf));
		if (k == NULL)
			ERR("!Malloc");

		return k;
	}

	struct critnib_leaf *k = c->deleted_leaf;

	c->deleted_leaf = k->value;
	VALGRIND_ANNOTATE_NEW_MEMORY(k, sizeof(*k));

	return k;
}

/*
 * crinib_insert -- write a key:value pair to the critnib structure
 *
 * Returns:
 *  • 0 on success
 *  • EEXIST if such a key already exists
 *  • ENOMEM if we're out of memory
 *
 * Takes a global write lock but doesn't stall any readers.
 */
int
critnib_insert(struct critnib *c, uint64_t key, void *value)
{
	util_mutex_lock(&c->mutex);

	struct critnib_leaf *k = alloc_leaf(c);
	if (!k) {
		util_mutex_unlock(&c->mutex);

		return ENOMEM;
	}

	VALGRIND_HG_DRD_DISABLE_CHECKING(k, sizeof(struct critnib_leaf));

	k->key = key;
	k->value = value;

	struct critnib_node *kn = (void *)((uint64_t)k | 1);

	struct critnib_node *n = c->root;
	if (!n) {
		c->root = kn;

		util_mutex_unlock(&c->mutex);

		return 0;
	}

	struct critnib_node **parent = &c->root;
	struct critnib_node *prev = c->root;

	while (n && !is_leaf(n) && (key & path_mask(n->shift)) == n->path) {
		prev = n;
		parent = &n->child[slice_index(key, n->shift)];
		n = *parent;
	}

	if (!n) {
		n = prev;
		store(&n->child[slice_index(key, n->shift)], kn);

		util_mutex_unlock(&c->mutex);

		return 0;
	}

	uint64_t path = is_leaf(n) ? to_leaf(n)->key : n->path;
	/* Find where the path differs from our key. */
	uint64_t at = path ^ key;
	if (!at) {
		ASSERT(is_leaf(n));
		free_leaf(c, to_leaf(kn));
		/* fail instead of replacing */

		util_mutex_unlock(&c->mutex);

		return EEXIST;
	}

	/* and convert that to an index. */
	sh_t sh = util_mssb_index64(at) & (sh_t)~(SLICE - 1);

	struct critnib_node *m = alloc_node(c);
	if (!m) {
		free_leaf(c, to_leaf(kn));

		util_mutex_unlock(&c->mutex);

		return ENOMEM;
	}
	VALGRIND_HG_DRD_DISABLE_CHECKING(m, sizeof(struct critnib_node));

	for (int i = 0; i < SLNODES; i++)
		m->child[i] = NULL;

	m->child[slice_index(key, sh)] = kn;
	m->child[slice_index(path, sh)] = n;
	m->shift = sh;
	m->path = key & path_mask(sh);
	store(parent, m);

	util_mutex_unlock(&c->mutex);

	return 0;
}

/*
 * critnib_remove -- delete a key from the critnib structure, return its value
 */
void *
critnib_remove(struct critnib *c, uint64_t key)
{
	struct critnib_leaf *k;
	void *value = NULL;

	util_mutex_lock(&c->mutex);

	struct critnib_node *n = c->root;
	if (!n)
		goto not_found;

	uint64_t del = util_fetch_and_add64(&c->remove_count, 1) % DELETED_LIFE;
	free_node(c, c->pending_del_nodes[del]);
	free_leaf(c, c->pending_del_leaves[del]);
	c->pending_del_nodes[del] = NULL;
	c->pending_del_leaves[del] = NULL;

	if (is_leaf(n)) {
		k = to_leaf(n);
		if (k->key == key) {
			store(&c->root, NULL);
			goto del_leaf;
		}

		goto not_found;
	}
	/*
	 * n and k are a parent:child pair (after the first iteration); k is the
	 * leaf that holds the key we're deleting.
	 */
	struct critnib_node **k_parent = &c->root;
	struct critnib_node **n_parent = &c->root;
	struct critnib_node *kn = n;

	while (!is_leaf(kn)) {
		n_parent = k_parent;
		n = kn;
		k_parent = &kn->child[slice_index(key, kn->shift)];
		kn = *k_parent;

		if (!kn)
			goto not_found;
	}

	k = to_leaf(kn);
	if (k->key != key)
		goto not_found;

	store(&n->child[slice_index(key, n->shift)], NULL);

	/* Remove the node if there's only one remaining child. */
	int ochild = -1;
	for (int i = 0; i < SLNODES; i++) {
		if (n->child[i]) {
			if (ochild != -1)
				goto del_leaf;

			ochild = i;
		}
	}

	ASSERTne(ochild, -1);

	store(n_parent, n->child[ochild]);
	c->pending_del_nodes[del] = n;

del_leaf:
	value = k->value;
	c->pending_del_leaves[del] = k;

not_found:
	util_mutex_unlock(&c->mutex);
	return value;
}

/*
 * critnib_get -- query for a key ("==" match), returns value or NULL
 *
 * Doesn't need a lock but if many deletes happened while our thread was
 * somehow stalled the query is restarted (as freed nodes remain unused only
 * for a grace period).
 *
 * Counterintuitively, it's pointless to return the most current answer,
 * we need only one that was valid at any point after the call started.
 */
void *
critnib_get(struct critnib *c, uint64_t key)
{
	uint64_t wrs1, wrs2;
	void *res;

	do {
		struct critnib_node *n;

		load(&c->remove_count, &wrs1);
		load(&c->root, &n);

		/*
		 * critbit algorithm: dive into the tree, looking at nothing but
		 * each node's critical bit^H^H^Hnibble.  This means we risk
		 * going wrong way if our path is missing, but that's ok...
		 */
		while (n && !is_leaf(n))
			load(&n->child[slice_index(key, n->shift)], &n);

		/* ... as we check it at the end. */
		struct critnib_leaf *k = to_leaf(n);
		res = (n && k->key == key) ? k->value : NULL;
		load(&c->remove_count, &wrs2);
	} while (wrs1 + DELETED_LIFE <= wrs2);

	return res;
}

/*
 * internal: find_successor -- return the rightmost non-null node in a subtree
 */
static void *
find_successor(struct critnib_node *__restrict n)
{
	while (1) {
		int nib;
		for (nib = NIB; nib >= 0; nib--)
			if (n->child[nib])
				break;

		if (nib < 0)
			return NULL;

		n = n->child[nib];
		if (is_leaf(n))
			return to_leaf(n)->value;
	}
}

/*
 * internal: find_le -- recursively search <= in a subtree
 */
static void *
find_le(struct critnib_node *__restrict n, uint64_t key)
{
	if (!n)
		return NULL;

	if (is_leaf(n)) {
		struct critnib_leaf *k = to_leaf(n);

		return (k->key <= key) ? k->value : NULL;
	}

	/*
	 * is our key outside the subtree we're in?
	 *
	 * If we're inside, all bits above the nib will be identical; note
	 * that shift points at the nib's lower rather than upper edge, so it
	 * needs to be masked away as well.
	 */
	if ((key ^ n->path) >> (n->shift) & ~NIB) {
		/*
		 * subtree is too far to the left?
		 * -> its rightmost value is good
		 */
		if (n->path < key)
			return find_successor(n);

		/*
		 * subtree is too far to the right?
		 * -> it has nothing of interest to us
		 */
		return NULL;
	}

	unsigned nib = slice_index(key, n->shift);
	/* recursive call: follow the path */
	{
		struct critnib_node *m;
		load(&n->child[nib], &m);
		void *value = find_le(m, key);
		if (value)
			return value;
	}

	/*
	 * nothing in that subtree?  We strayed from the path at this point,
	 * thus need to search every subtree to our left in this node.  No
	 * need to dive into any but the first non-null, though.
	 */
	for (; nib > 0; nib--) {
		struct critnib_node *m;
		load(&n->child[nib - 1], &m);
		if (m) {
			n = m;
			if (is_leaf(n))
				return to_leaf(n)->value;

			return find_successor(n);
		}
	}

	return NULL;
}

/*
 * critnib_find_le -- query for a key ("<=" match), returns value or NULL
 *
 * Same guarantees as critnib_get().
 */
void *
critnib_find_le(struct critnib *c, uint64_t key)
{
	uint64_t wrs1, wrs2;
	void *res;

	do {
		load(&c->remove_count, &wrs1);
		struct critnib_node *n; /* avoid a subtle TOCTOU */
		load(&c->root, &n);
		res = n ? find_le(n, key) : NULL;
		load(&c->remove_count, &wrs2);
	} while (wrs1 + DELETED_LIFE <= wrs2);

	return res;
}
