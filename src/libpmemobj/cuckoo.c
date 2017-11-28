/*
 * Copyright 2015-2017, Intel Corporation
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
 * cuckoo.c -- implementation of cuckoo hash table
 */
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sched.h>

#include "cuckoo.h"
#include "out.h"
#include "sys_util.h"

#define MAX_HASH_FUNCS 2

#define GROWTH_FACTOR 1.2f
#define INITIAL_SIZE 8
#define MAX_INSERTS 8
#define MAX_GROWS 32

struct cuckoo_slot {
	uint64_t key;
	void *value;
};

struct cuckoo {
	volatile size_t size; /* number of hash table slots */

	struct cuckoo_slot *volatile tab; /* the actual table */

	/* used for detecting concurrent modifications */
	volatile uint32_t status;

	/* Multi thread safety */
	enum cuckoo_mt_policy mt_policy;

	/*
	 * The following three fields are only used
	 * when mt_policy == cuckoo_mt_safe
	 */
	os_mutex_t lock;
	uint16_t old_tab_count; /* number of deffered deallocations */
	void **old_tabs; /* an array of pointers to Free */
};

static const struct cuckoo_slot null_slot = {0, NULL};

/*
 * status_load -- (internal) read the status field
 */
static uint32_t
status_load(struct cuckoo *c)
{
	uint32_t value;
	util_atomic_load_explicit32(&c->status, &value, memory_order_acquire);
	return value;
}

/*
 * status_increment -- (internal) increment the integer in the status field
 */
static void
status_increment(struct cuckoo *c)
{
	util_fetch_and_add32(&c->status, 1);
}

/*
 * modification_begin -- (internal) must be called before modifications, to
 * provide MT safety if needed.
 */
static void
modification_begin(struct cuckoo *c)
{
	if (c->mt_policy == cuckoo_mt_safe) {
		util_mutex_lock(&c->lock);
		status_increment(c);
	}
}

/*
 * modification_end -- (internal) must be called after modifications, to
 * provide MT safety if needed.
 */
static void
modification_end(struct cuckoo *c)
{
	if (c->mt_policy == cuckoo_mt_safe) {
		status_increment(c);
		util_mutex_unlock(&c->lock);
	}
}

/*
 * hash_mod -- (internal) first hash function
 */
static size_t
hash_mod(struct cuckoo *c, uint64_t key)
{
	return key % c->size;
}

/*
 * hash_mixer -- (internal) second hash function
 *
 * Based on Austin Appleby MurmurHash3 64-bit finalizer.
 */
static size_t
hash_mixer(struct cuckoo *c, uint64_t key)
{
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccd;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53;
	key ^= key >> 33;
	return key % c->size;
}

static size_t
(*hash_funcs[MAX_HASH_FUNCS])(struct cuckoo *c, uint64_t key) = {
	hash_mod,
	hash_mixer
};

/*
 * cuckoo_new -- allocates and initializes cuckoo hash table
 */
struct cuckoo *
cuckoo_new(enum cuckoo_mt_policy mt_policy)
{
	COMPILE_ERROR_ON((size_t)(INITIAL_SIZE * GROWTH_FACTOR)
		== INITIAL_SIZE);

	if (mt_policy != cuckoo_mt_safe && mt_policy != cuckoo_mt_dangerous) {
		errno = EINVAL;
		return NULL;
	}

	struct cuckoo *c = Zalloc(sizeof(struct cuckoo));
	if (c == NULL) {
		ERR("!Malloc");
		goto error_cuckoo_malloc;
	}

	c->size = INITIAL_SIZE;
	size_t tab_rawsize = c->size * sizeof(struct cuckoo_slot);
	c->tab = Zalloc(tab_rawsize);
	if (c->tab == NULL)
		goto error_tab_malloc;

	c->mt_policy = mt_policy;
	if (mt_policy == cuckoo_mt_safe)
		util_mutex_init(&c->lock);

	return c;

error_tab_malloc:
	Free(c);
error_cuckoo_malloc:
	return NULL;
}

/*
 * cuckoo_deallocate_old_tab -- (internal) deallocates, or marks for
 * deallocation a previously allocated, now unused region of memory
 */
static void
cuckoo_deallocate_old_tab(struct cuckoo *c, struct cuckoo_slot *old_tab)
{
	if (c->mt_policy == cuckoo_mt_safe) {
		c->old_tabs[c->old_tab_count] = old_tab;
		c->old_tab_count++;
	} else {
		Free(old_tab);
	}
}

/*
 * cuckoo_delete -- cleanups and deallocates cuckoo hash table
 * When deferred deallocation is enabled, Free is only called
 * is this routine.
 */
void
cuckoo_delete(struct cuckoo *c)
{
	ASSERTne(c, NULL);
	Free(c->tab);
	if (c->mt_policy == cuckoo_mt_safe) {
		for (uint16_t i = 0; i < c->old_tab_count; ++i)
			Free(c->old_tabs[i]);
		Free(c->old_tabs);
		util_mutex_destroy(&c->lock);
	}
	Free(c);
}

/*
 * cuckoo_insert_try -- (internal) try inserting into the existing hash table
 */
static int
cuckoo_insert_try(struct cuckoo *c, struct cuckoo_slot *src)
{
	struct cuckoo_slot srct;
	struct cuckoo_slot *tab = c->tab;
	size_t h[MAX_HASH_FUNCS] = {0};
	for (int n = 0; n < MAX_INSERTS; ++n) {
		for (int i = 0; i < MAX_HASH_FUNCS; ++i) {
			h[i] = hash_funcs[i](c, src->key);
			if (tab[h[i]].value == NULL) {
				tab[h[i]] = *src;
				return 0;
			} else if (tab[h[i]].key == src->key) {
				return EINVAL;
			}
		}

		srct = tab[h[0]];
		tab[h[0]] = *src;
		src->key = srct.key;
		src->value = srct.value;
	}

	return EAGAIN;
}

/*
 * cuckoo_update_on_grow -- (internal) updates the observable state of the table
 * to point to the newly rehash internal tab
 *
 * The only two fields which matter in this regard, are `tab` and `size`,
 * as cuckoo_get does only access these.
 */
static void
cuckoo_update_on_grow(struct cuckoo *observable_state, struct cuckoo *new_state)
{
	util_atomic_store_explicit64(&observable_state->tab, new_state->tab,
			memory_order_release);
	/*
	 * At this point, other threads might be accessing the
	 * newly built table, using the old size. Since the size
	 * of this table never decreases, such accesses always
	 * refer to accessible memory. The values read from
	 * that memory are unspecified, but this is indicated
	 * using the status field of the cuckoo struct.
	 */
	util_atomic_store_explicit64(&observable_state->size, new_state->size,
			memory_order_release);
}

/*
 * cuckoo_grow -- (internal) rehashes the table with twice the size
 *
 * The rehashed table is not visible to other threads while it is under
 * construction. This routine only modifies anything observable to other threads
 * once it reaches a point where no more failures are possible. The
 * modifications are applied to the actual visible state of the cuckoo in a
 * specific order, to make sure other threads accessing it do not dereference
 * invalid pointers.
 */
static int
cuckoo_grow(struct cuckoo *observable)
{
	struct cuckoo c = *observable;
	if (c.mt_policy == cuckoo_mt_safe) {
		/* Make sure there is enough space to hold pointers for later */
		size_t old_tabs_nsize = sizeof(*c.tab) * (c.old_tab_count + 1u);
		c.old_tabs = Realloc(c.old_tabs, old_tabs_nsize);
		if (c.old_tabs == NULL)
			return ENOMEM;
		observable->old_tabs = c.old_tabs;
	}

	struct cuckoo_slot *old_tab = observable->tab;
	size_t old_size = observable->size;
	for (int n = 0; n < MAX_GROWS; ++n) {
		c.size = (size_t)((float)c.size * GROWTH_FACTOR);

		c.tab = Zalloc(c.size * sizeof(c.tab[0]));
		if (c.tab == NULL)
			return ENOMEM;

		size_t i;
		for (i = 0; i < old_size; ++i) {
			struct cuckoo_slot s = old_tab[i];
			if (s.value != NULL && (cuckoo_insert_try(&c, &s) != 0))
				break;
		}

		if (i == old_size) {
			cuckoo_update_on_grow(observable, &c);
			cuckoo_deallocate_old_tab(observable, old_tab);
			return 0;
		}

		Free(c.tab);
	}

	return EINVAL;
}

/*
 * cuckoo_insert -- inserts key-value pair into the hash table
 */
int
cuckoo_insert(struct cuckoo *c, uint64_t key, void *value)
{
	ASSERTne(c, NULL);
	int err = 0;
	int n;
	struct cuckoo_slot src = {key, value};

	modification_begin(c);

	for (n = 0; n < MAX_GROWS; ++n) {
		if ((err = cuckoo_insert_try(c, &src)) != EAGAIN)
			break;

		if ((err = cuckoo_grow(c)) != 0)
			break;
	}

	modification_end(c);

	if (n == MAX_GROWS)
		err = EINVAL;

	return err;
}

/*
 * cuckoo_find_slot -- (internal) finds the hash table slot of key
 */
static struct cuckoo_slot *
cuckoo_find_slot(struct cuckoo *c, uint64_t key)
{
	for (int i = 0; i < MAX_HASH_FUNCS; ++i) {
		size_t h = hash_funcs[i](c, key);
		if (c->tab[h].key == key)
			return &c->tab[h];
	}

	return NULL;
}

/*
 * cuckoo_remove -- removes key-value pair from the hash table
 */
void *
cuckoo_remove(struct cuckoo *c, uint64_t key)
{
	ASSERTne(c, NULL);
	void *ret = NULL;
	struct cuckoo_slot *s = cuckoo_find_slot(c, key);
	if (s) {
		ret = s->value;

		modification_begin(c);
		*s = null_slot;
		modification_end(c);
	}

	return ret;
}

/*
 * indicates_modification_in_progress -- (internal) checks if the integer value
 * indicates an ongoing concurrent modification.
 * The routines Inserting into, and removing from the table both increment the
 * integer stored in the status field by one before and after performing the
 * operation. This means, if that integer is odd (bit #0 is set), a modification
 * is started, and if it is even (bit #0 is clear), no modification is in
 * progress.
 */
static bool
indicates_modification_in_progress(uint32_t status)
{
	return (status & 1) != 0;
}

/*
 * cuckoo_try_lf_get -- Returns the value of a key, as long as
 * it does not detect concurrent any modifications.
 *
 * It is important to avoid optimizations that would hoist the load of the
 * status field from after the actual lookup to before the cuckoo_find_slot
 * call, or hoist the one from before the cuckoo_find_slot call to after it.
 * These two loads (with acquire semantics) must surround the actual lookup,
 * while any modifications that can affect the lookup code must be surrounded
 * stores (with release semantics) to the status field.
 *
 * This is a way to deal with possible races, with CAS or similar operations,
 * thus allowing the code to use lower level caches when cuckoo_get is
 * frequently used. The frequent use here mainly assumes using it as the map
 * between uuid_lo values and pointers to pools, which is a map that only
 * ever changes when a pool is opened/created/closed.
 */
static int
cuckoo_get_try(struct cuckoo *c, uint64_t key, void **value)
{
	uint32_t status_seen = status_load(c);
	if (indicates_modification_in_progress(status_seen))
		return -1;

	struct cuckoo_slot *s = cuckoo_find_slot(c, key);
	if (s != NULL)
		*value = s->value;
	else
		*value = NULL;

	if (status_seen != status_load(c))
		return -1;

	return 0;
}

static void
os_yield(void)
{
	/* XXX clean this up, move to os_* layer */
#ifdef _WIN32
	SwitchToThread();
#else
	sched_yield();
#endif
}

/*
 * cuckoo_get -- returns the value of a key
 *
 * When deferred allocation is enabled, cuckoo_get can be used concurrently
 * with cuckoo_insert or cuckoo_remove. It is important however, that using
 * cuckoo_insert and cuckoo_remove still conflict with each othere, one must
 * apply mutual exclusion when using those.
 *
 * The cuckoo_get function uses no locking, no syscalls unless it detects
 * the need for such.
 *
 * The fastpath in this function consists of the steps:
 *  1) checking the status field
 *  2) performing the requested lookup
 *  3) checking the status field again
 *  4) returning the result from step #2
 *
 * The status field can indicate wether the underlying data being read from
 * memory can be trusted or not. If it is not safe to use the data read during
 * step #2 (only happens while a cuckoo_insert, or cuckoo_remove is executed
 * on another thread), the slow path is used, which basically just means
 * restarting the fast-path from step #1.
 */
void *
cuckoo_get(struct cuckoo *c, uint64_t key)
{
	ASSERTne(c, NULL);
	void *result;

	while (cuckoo_get_try(c, key, &result) != 0)
		os_yield();

	return result;
}

/*
 * cuckoo_get_size -- returns the size of the underlying table, useful for
 *	calculating load factor and predicting possible rehashes
 */
size_t
cuckoo_get_size(struct cuckoo *c)
{
	ASSERTne(c, NULL);
	return c->size;
}
