/*
 * Copyright 2018, Intel Corporation
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
 * tree).  Unlike the cuckoo hash, the complexity isn't constant, but for
 * data sizes we expect it's several times as fast as cuckoo, and has no
 * "stop the world" cases that would cause latency (ie, better worst case
 * behaviour).
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

#include "critnib.h"
#include "os_thread.h"
#include "out.h"

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
 * started and ended.
 *
 */
#define DELETED_LIFE 16
#define ENDBIT (-1)
#define UNLOCK os_mutex_unlock(&c->mutex)

#define SLICE 4
#define NIB ((1UL << SLICE) - 1)
#define SLNODES (1 << SLICE)

struct critnib_node {
	struct critnib_node *child[SLNODES];
	uint64_t path;
	int32_t shift;
};

struct critnib {
	struct critnib_node *root;
	struct critnib_node *deleted_node;
	struct critnib_node *pending_dels[DELETED_LIFE][2];
	uint64_t volatile write_status;
	os_mutex_t mutex;
};

static struct critnib_node nullnode = {{0, }, 0, ENDBIT};

/*
 * critnib_new -- allocates a new critnib structure
 */
struct critnib *
critnib_new(void)
{
	struct critnib *c = Zalloc(sizeof(struct critnib));
	if (!c)
		return 0;
	c->root = &nullnode;
	os_mutex_init(&c->mutex);
	return c;
}

/*
 * internal: delete_node -- recursively free (to malloc) a subtree
 */
static void
delete_node(struct critnib_node *__restrict n)
{
	if (n == &nullnode)
		return;

	if (n->shift != ENDBIT)
		for (int i = 0; i < SLNODES; i++) {
			if (n->child[i])
				delete_node(n->child[i]);
		}
	Free(n);
}

/*
 * critnib_delete -- destroy and free a critnib struct
 */
void
critnib_delete(struct critnib *c)
{
	if (c->root)
		delete_node(c->root);
	os_mutex_destroy(&c->mutex);
	for (struct critnib_node *m = c->deleted_node; m; ) {
		struct critnib_node *mm = m->child[0];
		Free(m);
		m = mm;
	}
	for (int i = 0; i < DELETED_LIFE; i++)
		for (int j = 0; j < 2; j++) {
			if (c->pending_dels[i][j])
				Free(c->pending_dels[i][j]);
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
	n->child[0] = c->deleted_node;
	c->deleted_node = n;
}

/*
 * internal: alloc_node -- allocate a node from our pool or from malloc
 */
static struct critnib_node *
alloc_node(struct critnib *__restrict c)
{
	if (!c->deleted_node)
		return Malloc(sizeof(struct critnib_node));
	struct critnib_node *n = c->deleted_node;
	c->deleted_node = n->child[0];
	return n;
}

/*
 * crinib_insert -- write a key:value pair to the critnib structure
 *
 * Returns:
 *  • 0 on success
 *  • EINVAL if such a key already exists
 *  • ENOMEM if we're out of memory
 *
 * Takes a global write lock but doesn't stall any readers.
 */
int
critnib_insert(struct critnib *c, uint64_t key, void *value)
{
	os_mutex_lock(&c->mutex);
	struct critnib_node *k = alloc_node(c);
	if (!k)
		return UNLOCK, ENOMEM;
	k->path = key;
	k->shift = ENDBIT;
	k->child[0] = value;

	struct critnib_node *n = c->root;
	if (n == &nullnode) {
		c->root = k;
		return UNLOCK, 0;
	}

	struct critnib_node **parent = &c->root, *prev = c->root;

	while (n->shift != ENDBIT && (key & (~NIB << n->shift)) == n->path) {
		prev = n;
		parent = &n->child[(key >> n->shift) & NIB];
		n = *parent;
	}

	if (n == &nullnode) {
		n = prev;
		util_atomic_store_explicit64(
			(uint64_t *)&n->child[(key >> n->shift) & NIB],
			(uint64_t)k, memory_order_release);
		return UNLOCK, 0;
	}

	uint64_t at = n->path ^ key;
	if (!at) {
		ASSERTeq(n->shift, ENDBIT);
		free_node(c, k);
		/* fail instead of replacing */
		return UNLOCK, EINVAL;
	}
	int32_t sh = util_mssb_index64(at) & ~(SLICE - 1);

	struct critnib_node *m = alloc_node(c);
	if (!m) {
		free_node(c, k);
		return UNLOCK, ENOMEM;
	}

	for (int i = 0; i < SLNODES; i++)
		m->child[i] = &nullnode;
	uint64_t dir = (key >> sh) & NIB;
	m->child[dir] = k;
	m->child[(n->path >> sh) & NIB] = n;
	m->shift = sh;
	m->path = key & (~NIB << sh);
	util_atomic_store_explicit64((uint64_t *)parent, (uint64_t)m,
		memory_order_release);

	return UNLOCK, 0;
}

/*
 * critnib_remove -- delete a key from the critnib structure, return its value
 */
void *
critnib_remove(struct critnib *c, uint64_t key)
{
	os_mutex_lock(&c->mutex);

	struct critnib_node *n = c->root;
	if (!n)
		return UNLOCK, NULL;
	uint64_t del = util_fetch_and_add64(&c->write_status, 1) % DELETED_LIFE;
	free_node(c, c->pending_dels[del][0]);
	free_node(c, c->pending_dels[del][1]);
	c->pending_dels[del][1] = c->pending_dels[del][0] = 0;
	if (n->shift == ENDBIT) {
		if (n->path == key && n != &nullnode) {
			util_atomic_store_explicit64((uint64_t *)&c->root,
				(uint64_t)&nullnode, memory_order_release);
			void *value = n->child[0];
			c->pending_dels[del][0] = n;
			return UNLOCK, value;
		}
		return UNLOCK, NULL;
	}
	struct critnib_node **k_parent = &c->root, **n_parent = &c->root,
			    *k = n;

	while (k->shift != ENDBIT) {
		n_parent = k_parent;
		n = k;
		k_parent = &k->child[(key >> k->shift) & NIB];
		k = *k_parent;
	}
	if (k->path != key)
		return UNLOCK, NULL;

	util_atomic_store_explicit64(
		(uint64_t *)&n->child[(key >> n->shift) & NIB],
		(uint64_t)&nullnode, memory_order_release);

	int ochild = -1;
	for (int i = 0; i < SLNODES; i++)
		if (n->child[i] != &nullnode) {
			if (ochild != -1) {
				void *value = k->child[0];
				c->pending_dels[del][0] = k;
				return UNLOCK, value;
			} else
				ochild = i;
		}
	if (ochild == -1)
		ochild = 0;

	util_atomic_store_explicit64((uint64_t *)n_parent,
		(uint64_t)n->child[ochild], memory_order_release);
	void *value = k->child[0];
	c->pending_dels[del][0] = n;
	c->pending_dels[del][1] = k;
	return UNLOCK, value;
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
		util_atomic_load_explicit64(&c->write_status, &wrs1,
			memory_order_acquire);
		struct critnib_node *n = c->root;
		if (!n)
			return 0;
		/*
		 * critbit algorithm: dive into the tree, looking at nothing but
		 * each node's critical bit^H^H^Hnibble.  This means we risk
		 * going wrong way if our path is missing, but that's ok...
		 */
		while (n->shift != ENDBIT)
			n = n->child[(key >> n->shift) & NIB];
		/* ... as we check it at the end. */
		res = (n->path == key) ? n->child[0] : 0;
		util_atomic_load_explicit64(&c->write_status, &wrs2,
			memory_order_acquire);
	} while (wrs1 + DELETED_LIFE <= wrs2);
	return res;
}

/*
 * internal: find_le_dive -- return the rightmost non-null node in a subtree
 */
static void *
find_le_dive(struct critnib_node *__restrict n, uint64_t key)
{
deeper:
	for (int nib = NIB; nib >= 0; nib--)
		if (n->child[nib] != &nullnode) {
			n = n->child[nib];
			if (n->shift == ENDBIT)
				return n->child[0];
			goto deeper;
		}
	return NULL;
}

/*
 * internal: find_le -- recursively search <= in a subtree
 */
static void *
find_le(struct critnib_node *__restrict n, uint64_t key)
{
	/* is this a leaf node? */
	if (n->shift == ENDBIT)
		return (n->path <= key) ? n->child[0] : NULL;

	/* are we in a subtree outside our path? */
	if ((key ^ n->path) >> (n->shift) & ~NIB) {
		/*
		 * subtree is too far to the left?
		 * -> its rightmost value is good
		 */
		if (n->path < key)
			return find_le_dive(n, key);
		/*
		 * subtree is too far to the right?
		 * -> it has nothing of interest to us
		 */
		return NULL;
	}

	int nib = (key >> n->shift) & NIB;
	/* recursive call: follow the path */
	void *value = find_le(n->child[nib], key);
	if (value)
		return value;
	/*
	 * nothing in that subtree?  We strayed from the path at this point,
	 * thus need to search every subtree to our left in this node.  No
	 * need to dive into any but the first non-null, though.
	 */
	for (nib--; nib >= 0; nib--)
		if (n->child[nib] != &nullnode) {
			n = n->child[nib];
			if (n->shift == ENDBIT)
				return n->child[0];
			return find_le_dive(n, key);
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
		util_atomic_load_explicit64(&c->write_status, &wrs1,
			memory_order_acquire);
		res = find_le(c->root, key);
		util_atomic_load_explicit64(&c->write_status, &wrs2,
			memory_order_acquire);
	} while (wrs1 + DELETED_LIFE <= wrs2);
	return res;
}
