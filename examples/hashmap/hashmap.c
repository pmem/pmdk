// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "libminiasync.h"
#include "libminiasync/data_mover_threads.h"

/* Avoid compatibility errors */
#ifndef _MSC_VER
#define bool_compare_and_swap __sync_bool_compare_and_swap
#define fetch_and_add __sync_fetch_and_add
#define fetch_and_sub __sync_fetch_and_sub
#else
#include <windows.h>
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

static __inline int
bool_compare_and_swap_MSVC(volatile LONG *ptr, LONG oldval, LONG newval)
{
	LONG old = InterlockedCompareExchange(ptr, newval, oldval);
	return (old == oldval);
}

#define bool_compare_and_swap(p, o, n)\
	bool_compare_and_swap_MSVC((LONG *)(p), (LONG)(o), (LONG)(n))
#define fetch_and_add(ptr, value)\
	InterlockedExchangeAdd((LONG *)(ptr), value)
#define fetch_and_sub(ptr, value)\
	InterlockedExchangeAdd((LONG *)(ptr), -value)
#endif

#define WAIT_FUTURES_MAX 4

/* Polls 'nfuts' number of futures until they complete, makes use of runtime */
#define WAIT_FUTURES(runtimep, futsp, nfuts)\
do {\
	struct future *tmp_futs[WAIT_FUTURES_MAX];\
	for (int i = 0; i < nfuts; i++) {\
		tmp_futs[i] = FUTURE_AS_RUNNABLE(&(futsp[i]));\
	}\
\
	runtime_wait_multiple(r, tmp_futs, nfuts);\
} while (0)

/*
 * Represents the state of hashmap entry. Each hashmap entry can be in one of
 * the following states:
 *
 * 'HASHMAP_ENTRY_STATE_UNOCCUPIED' - hashmap entry is not occupied,
 * 'HASHMAP_ENTRY_STATE_LOCKED' - hashmap entry is being locked,
 * 'HASHMAP_ENTRY_STATE_PRESENT' - hashmap entry contains a key-value pair
 */
enum hashmap_entry_state {
	HASHMAP_ENTRY_STATE_UNOCCUPIED,
	HASHMAP_ENTRY_STATE_LOCKED,
	HASHMAP_ENTRY_STATE_PRESENT,
};

/* Hashmap entry definition */
struct hashmap_entry {
	uint64_t key;
	struct {
		void *addr;
		size_t size;
	} value;
	enum hashmap_entry_state state;
};

struct hashmap {
	struct hashmap_entry *entries;
	size_t capacity; /* Max stored entries */
	size_t length; /* Current number of stored entries */
};

/* Hash function based on Austin Appleby MurmurHash3 64-bit finalizer */
static size_t
hash_val(uint64_t val)
{
	val ^= val >> 33;
	val *= 0xff51afd7ed558ccd;
	val ^= val >> 33;
	val *= 0xc4ceb9fe1a85ec53;
	val ^= val >> 33;

	return val;
}

/* Checks if entry is empty */
static inline int
hashmap_entry_empty(struct hashmap_entry *hme)
{
	return (hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED && hme->key == 0);
}

/* Checks if entry is deleted */
static inline int
hashmap_entry_deleted(struct hashmap_entry *hme)
{
	return (hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED && hme->key != 0);
}

/* Checks if hashmap entry is unoccupied */
static inline int
hashmap_entry_unoccupied(struct hashmap_entry *hme)
{
	return hme->state == HASHMAP_ENTRY_STATE_UNOCCUPIED;
}

/* Creates a new hashmap instance */
static struct hashmap *
hashmap_new(size_t capacity)
{
	if (capacity <= 0) {
		printf("hashmap capacity has to be bigger than 0\n");
		return NULL;
	}

	struct hashmap *hm = malloc(sizeof(struct hashmap));
	if (hm == NULL) {
		return NULL;
	}

	/* Allocate zero'd space */
	hm->entries = calloc(capacity, sizeof(struct hashmap_entry));
	if (hm->entries == NULL) {
		goto free_hm;
	}

	hm->capacity = capacity; /* Maximum number of hashmap entries */
	hm->length = 0; /* Number of currently occupied hashmap entries */

	return hm;

free_hm:
	free(hm);
	return NULL;
}

/* Deletes a hashmap instance */
void
hashmap_delete(struct hashmap *hm)
{
	for (size_t i = 0; i < hm->length; i++) {
		if (hm->entries[i].state == HASHMAP_ENTRY_STATE_PRESENT) {
			free(hm->entries[i].value.addr);
		}
	}

	free(hm->entries);
	free(hm);
}

/* Calculates the entry index based on provided key and hashmap capacity */
static size_t
hashmap_key_index(struct hashmap *hm, uint64_t key)
{
	return (hash_val(key) % (hm->capacity - 1));
}

/*
 * Looks for the entry fitting provided key based on the 'state':
 *
 * 'HASHMAP_ENTRY_STATE_PRESENT' - looks for entry that's present
 * 'HASHMAP_ENTRY_STATE_UNOCCUPIED' - looks for unoccupied entry
 */
static ssize_t
hashmap_entry_lookup(struct hashmap *hm, uint64_t key,
		enum hashmap_entry_state state)
{
	/* Start the search from index calculated based on the provided key */
	ssize_t index = (ssize_t)hashmap_key_index(hm, key);

	for (size_t i = 0; i < hm->capacity; i++,
			index = (index + 1) % (ssize_t)hm->capacity) {
		struct hashmap_entry *hme = &hm->entries[index];

		switch (state) {
			/* We look for the first unoccupied entry */
			case HASHMAP_ENTRY_STATE_UNOCCUPIED:
				if (hashmap_entry_unoccupied(hme)) {
					return index;
				}
				break;
			/* We look for the entry matching the provided key */
			case HASHMAP_ENTRY_STATE_PRESENT:
				if (hashmap_entry_deleted(hme)) {
					/* Deleted entries are skipped */
					continue;
				} else if (hashmap_entry_empty(hme)) {
					/* Empty entry terminates the search */
					return -1;
				}

				if (hme->key == key) {
					return index;
				}
				break;
			default:
				assert(0); /* Should not be reachable */
		}

	}

	return -1;
}

/*
 * Compares the current hashmap entry state with the provided 'old' state and
 * changes it to the 'new' state when the comparison was successful.
 */
static unsigned
hashmap_entry_set_state_impl(struct hashmap_entry *hme,
		enum hashmap_entry_state old, enum hashmap_entry_state new)
{
	return bool_compare_and_swap(&hme->state, old, new);
}

/*
 * BEGIN of hashmap_entry_set_state_fut
 */
struct hashmap_entry_set_state_data {
	struct hashmap_entry *hme;
	enum hashmap_entry_state old;
	enum hashmap_entry_state new;
};

struct hashmap_entry_set_state_output {
	unsigned changed;
};

FUTURE(hashmap_entry_set_state_fut, struct hashmap_entry_set_state_data,
		struct hashmap_entry_set_state_output);

/*
 * hashmap_entry_set_state_fut future task function. Sets the hashmap entry
 * state from the old to the new state.
 */
static enum future_state
hashmap_entry_set_state_wrapped(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_set_state_data *data =
			future_context_get_data(ctx);
	struct hashmap_entry_set_state_output *output =
			future_context_get_output(ctx);

	output->changed = hashmap_entry_set_state_impl(data->hme, data->old,
			data->new);

	return FUTURE_STATE_COMPLETE;
}

/* Creates and initializes a new hashmap_entry_set_state_fut future */
static struct hashmap_entry_set_state_fut
hashmap_entry_set_state(struct hashmap_entry *hme, enum hashmap_entry_state old,
		enum hashmap_entry_state new)
{
	struct hashmap_entry_set_state_fut future;

	/* Set input values */
	future.data.hme = hme;
	future.data.new = new;
	future.data.old = old;
	/* Set default output value */
	future.output.changed = 0;

	FUTURE_INIT(&future, hashmap_entry_set_state_wrapped);

	return future;
}
/*
 * END of hashmap_entry_set_state_fut
 */

/*
 * BEGIN of hashmap_lookup_fut future
 */
struct hashmap_lookup_data {
	struct hashmap *hm;
	uint64_t key;
	enum hashmap_entry_state state;
};

struct hashmap_lookup_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_lookup_fut, struct hashmap_lookup_data,
		struct hashmap_lookup_output);

/*
 * hashmap_lookup_fut future task function. Looks for appropriate hashmap entry
 * matching provided key.
 */
static enum future_state
hashmap_lookup_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_lookup_data *data =
			future_context_get_data(ctx);
	struct hashmap_lookup_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	uint64_t key = data->key;
	enum hashmap_entry_state state = data->state;

	struct hashmap_entry *hme = NULL;
	if (key == 0) {
		printf("invalid key %" PRIu64 "\n", key);
		goto set_output;
	} else if (state == HASHMAP_ENTRY_STATE_UNOCCUPIED &&
			hm->capacity == hm->length) {
		printf("no space left for key %" PRIu64 "\n", key);
		goto set_output;
	} else if (state == HASHMAP_ENTRY_STATE_UNOCCUPIED &&
			hashmap_entry_lookup(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT) != -1) {
		printf("key %" PRIu64 " already exists\n", key);
		goto set_output;
	}

	ssize_t index = hashmap_entry_lookup(hm, key, state);
	if (index == -1) {
		switch (state) {
			case HASHMAP_ENTRY_STATE_PRESENT:
			/* Entry with given key is not present in the hashmap */
				goto set_output;
			case HASHMAP_ENTRY_STATE_UNOCCUPIED:
			/*
			 * An unoccupied entry wasn't found despite hashmap not
			 * being full. Re-run the lookup future.
			 */
				return FUTURE_STATE_RUNNING;
			default:
				assert(0); /* Should not be reachable */
		}
	}

	hme = &hm->entries[index];

set_output:
	output->hme = hme;
	return FUTURE_STATE_COMPLETE;
}

/* Creates and initializes a new hashmap_lookup_fut future */
static struct hashmap_lookup_fut
hashmap_lookup(struct hashmap *hm, uint64_t key, enum hashmap_entry_state state)
{
	struct hashmap_lookup_fut future;
	/* Set input values */
	future.data.hm = hm;
	future.data.key = key;
	future.data.state = state;
	/* Set default output value */
	future.output.hme = NULL;

	FUTURE_INIT(&future, hashmap_lookup_impl);

	return future;
}
/*
 * END of hashmap_lookup_fut future
 */

/*
 * BEGIN of chain_entries_rerun_fut future
 */
struct chain_entries_rerun_data {
	struct future_chain_entry **entries;
	size_t n_entries;
};

struct chain_entries_rerun_output {
	uint64_t unused; /* Avoid compiled empty struct error */
};

FUTURE(chain_entries_rerun_fut, struct chain_entries_rerun_data,
		struct chain_entries_rerun_output);

/*
 * chain_entries_rerun_fut future task function. Marks provided chained future
 * entries as not 'processed'. This future should only be used as an entry
 * of chained future.
 */
static enum future_state
chain_entries_rerun_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct chain_entries_rerun_data *data = future_context_get_data(ctx);

	struct future_chain_entry **entries = data->entries;
	size_t n_entries = data->n_entries;

	unsigned rerun = 0;
	for (size_t i = 0; i < n_entries; i++) {
		struct future_chain_entry *entry = entries[i];
		if (entry) {
			entry->flags &= ~FUTURE_CHAIN_FLAG_ENTRY_PROCESSED;
			entry->future.context.state = FUTURE_STATE_RUNNING;
			rerun = 1;
		}
	}

	if (rerun) {
		/*
		 * When we re-run this future all not 'processed' future
		 * entries in a chained future will be re-run.
		 */
		return FUTURE_STATE_RUNNING;
	}

	return FUTURE_STATE_COMPLETE;
}

/* Creates and initializes a new chain_entries_rerun_fut future */
static struct chain_entries_rerun_fut
chain_entries_rerun(struct future_chain_entry **entries, size_t n_entries)
{
	struct chain_entries_rerun_fut future;
	/* Set input values */
	future.data.entries = entries;
	future.data.n_entries = n_entries;

	FUTURE_INIT(&future, chain_entries_rerun_impl);

	return future;
}
/*
 * END of chain_entries_rerun_fut future
 */

/*
 * BEGIN of hashmap_lookup_lock_entry_fut future
 */
struct hashmap_lookup_lock_entry_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_fut, lookup);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_state);
	FUTURE_CHAIN_ENTRY_LAST(struct chain_entries_rerun_fut, entries_rerun);
	struct future_chain_entry *entriesp[2];
};

struct hashmap_lookup_lock_entry_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_lookup_lock_entry_fut, struct hashmap_lookup_lock_entry_data,
		struct hashmap_lookup_lock_entry_output);

/*
 * Maps 'lookup' future entry output data to the 'set_state' future entry data.
 */
static void
lookup_to_set_state_map(struct future_context *lookup_ctx,
		struct future_context *set_state_ctx, void *arg)
{
	struct hashmap_lookup_output *lookup_output =
			future_context_get_output(lookup_ctx);
	struct hashmap_entry_set_state_data *set_state_data =
			future_context_get_data(set_state_ctx);
	struct hashmap_entry *hme = lookup_output->hme;

	if (hme == NULL) {
		/*
		 * Entry lookup failed, no need to lock the entry in
		 * 'locked' state.
		 */
		set_state_ctx->state = FUTURE_STATE_COMPLETE;
	}

	set_state_data->hme = hme;
}

/*
 * Lazy initialization function. Initializes the 'chain_entries_rerun' future
 * based on the 'lookup' and 'set_state' future entries results.
 */
static void
chain_entry_rerun_init(void *future,
		struct future_context *lookup_lock_entry_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_data *data =
			future_context_get_data(lookup_lock_entry_ctx);
	struct hashmap_lookup_lock_entry_output *output =
			future_context_get_output(lookup_lock_entry_ctx);

	struct chain_entries_rerun_fut fut;

	struct hashmap_entry *hme = data->lookup.fut.output.hme;
	unsigned locked = data->set_state.fut.output.changed;
	if (hme != NULL && !locked) {
		/*
		 * 'lookup' found a hashmap entry, but 'set_state' failed to
		 * lock it. We should re-run those two future entries to try
		 * and find and lock a hashmap entry again.
		 */
		data->entriesp[0] = (struct future_chain_entry *)&data->lookup;
		data->entriesp[1] =
				(struct future_chain_entry *)&data->set_state;
		fut = chain_entries_rerun(data->entriesp, 2);
	} else {
		/*
		 * Either 'lookup' and 'set_state' successfuly found and locked
		 * a hashmap entry or the 'lookup' failed. 'chain_entry_rerun'
		 * shouldn't be executed.
		 */
		FUTURE_INIT_COMPLETE(&fut);

		/*
		 * At this point we can also set 'hashmap_lookup_lock_entry_fut'
		 * future output, since it practically finished its job.
		 */
		output->hme = data->lookup.fut.output.hme;
	}

	memcpy(future, &fut, sizeof(fut));
}

/* Creates and initializes a new hashmap_lookup_lock_entry_fut future */
static struct hashmap_lookup_lock_entry_fut
hashmap_lookup_lock_entry(struct hashmap *hm, uint64_t key,
		enum hashmap_entry_state state)
{
	struct hashmap_lookup_lock_entry_fut chain;
	/* Initialize chained future entries */
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup,
			hashmap_lookup(hm, key, state),
			lookup_to_set_state_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.set_state,
			hashmap_entry_set_state(NULL, state,
					HASHMAP_ENTRY_STATE_LOCKED),
			NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.entries_rerun,
			chain_entry_rerun_init, NULL, NULL, NULL);
	/* Set default chained future output value */
	chain.output.hme = NULL;

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_lookup_lock_entry_fut future
 */

/*
 * BEGIN of hashmap_get_fut future
 */
struct hashmap_get_copy_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy_value);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_state);
};

struct hashmap_get_copy_output {
	void *value;
	size_t size;
	size_t copy_size;
};

FUTURE(hashmap_get_copy_fut, struct hashmap_get_copy_data,
		struct hashmap_get_copy_output);

/*
 * Maps 'lookup_lock' future entry data to the 'memcpy_value' future entry
 * data.
 */
static void
lookup_lock_entry_to_memcpy_value_map(
		struct future_context *lookup_lock_entry_ctx,
		struct future_context *memcpy_value_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_output *lookup_lock_entry_output =
			future_context_get_output(lookup_lock_entry_ctx);
	struct vdm_operation_data *memcpy_value_data =
			future_context_get_data(memcpy_value_ctx);

	struct vdm_operation *memcpy_op = &memcpy_value_data->operation;

	struct hashmap_entry *hme = lookup_lock_entry_output->hme;
	if (hme == NULL) {
		/*
		 * 'lookup_lock_entry' failed to find or lock a hashmap entry.
		 * 'memcpy_value' shouldn't be executed.
		 */
		memcpy_value_ctx->state = FUTURE_STATE_COMPLETE;
		return;
	}

	size_t entry_size = hme->value.size;
	size_t buf_size = (size_t)arg;

	/* We will not copy more data than buffer can fit */
	if (entry_size > buf_size) {
		memcpy_op->data.memcpy.n = buf_size;
	} else {
		memcpy_op->data.memcpy.n = entry_size;
	}

	memcpy_op->data.memcpy.src = hme->value.addr;
}

/*
 * Maps 'set_state' future entry data to the 'hashmap_get_copy_fut' chained
 * future output data.
 */
static void
set_state_to_output_map_for_get(struct future_context *set_state_ctx,
		struct future_context *get_copy_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *set_state_data =
			future_context_get_data(set_state_ctx);
	struct hashmap_entry_set_state_output *set_state_output =
			future_context_get_output(set_state_ctx);
	struct hashmap_get_copy_data *get_copy_data =
			future_context_get_data(get_copy_ctx);
	struct hashmap_get_copy_output *get_copy_output =
			future_context_get_output(get_copy_ctx);

	struct vdm_operation_data_memcpy memcpy_value_data =
		get_copy_data->memcpy_value.fut.data.operation.data.memcpy;

	if (set_state_output->changed) {
		/*
		 * 'set_state' changed the hashmap entry state from 'locked'.
		 * We can assume that 'lookup_lock_entry' and 'memcpy_value'
		 * were successful, so we can set the 'hashmap_get_copy_fut'
		 * output safely.
		 */
		get_copy_output->value = memcpy_value_data.dest;
		get_copy_output->copy_size = memcpy_value_data.n;
		get_copy_output->size = set_state_data->hme->value.size;
	}
}

/*
 * Lazy initialization function. Initializes the 'set_state' future based on
 * the 'lookup_lock_entry' future entry result.
 */
static void
set_state_init_for_get(void *future,
		struct future_context *hashmap_get_copy_ctx, void *arg)
{
	struct hashmap_get_copy_data *data =
			future_context_get_data(hashmap_get_copy_ctx);

	struct hashmap_entry_set_state_fut fut = {.output.changed = 0};
	struct hashmap_entry *hme = data->lookup_lock_entry.fut.output.hme;
	if (hme == NULL) {
		/*
		 * 'lookup_lock_entry' future entry failed to find a hashmap
		 * entry. 'set_state' future entry shouldn't be executed.
		 */
		FUTURE_INIT_COMPLETE(&fut);
	} else {
		/*
		 * 'lookup_lock_entry' was successful.
		 * Set hashmap entry state to 'present'.
		 */
		fut = hashmap_entry_set_state(hme,
				HASHMAP_ENTRY_STATE_LOCKED,
				HASHMAP_ENTRY_STATE_PRESENT);
	}

	memcpy(future, &fut, sizeof(fut));
}

/* Creates and initializes a new hashmap_get_copy_fut future */
static struct hashmap_get_copy_fut
hashmap_get_copy(struct vdm *vdm, struct hashmap *hm, uint64_t key, void *buf,
		size_t buf_size)
{
	struct hashmap_get_copy_fut chain;
	/* Initialize chained future entries */
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT),
					lookup_lock_entry_to_memcpy_value_map,
					(void *)buf_size);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy_value,
			vdm_memcpy(vdm, buf, NULL, 0, 0), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_state,
			set_state_init_for_get, NULL,
			set_state_to_output_map_for_get, NULL);
	/* Set default output values */
	chain.output.size = 0;
	chain.output.copy_size = 0;
	chain.output.value = NULL;

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_get_fut future
 */

/*
 * BEGIN of hashmap_entry_init_fut future
 */
struct hashmap_entry_init_data {
	struct hashmap *hm;
	struct hashmap_entry *hme;
	uint64_t key;
	size_t size;
};

struct hashmap_entry_init_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_entry_init_fut, struct hashmap_entry_init_data,
		struct hashmap_entry_init_output);

/*
 * hashmap_entry_init_fut future task function. Initializes a hashmap entry with
 * an appropriate data and allocates memory for the hashmap entry value.
 */
static enum future_state
hashmap_entry_init_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_init_data *data = future_context_get_data(ctx);
	struct hashmap_entry_init_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	struct hashmap_entry *hme = data->hme;
	output->hme = hme;

	/* Allocate the memory for a new key-value pair */
	void *addr = malloc(data->size);
	if (addr == NULL) {
		return FUTURE_STATE_COMPLETE;
	}

	hme->key = data->key;
	hme->value.addr = addr;
	hme->value.size = data->size;

	/* Increase the hashmap length */
	size_t old_val = fetch_and_add(&hm->length, 1);
	assert(old_val != hm->length);

	/* Avoid unused variable warning */
	(void) old_val;

	return FUTURE_STATE_COMPLETE;
}

/* Creates and initializes a new hashmap_entry_init_fut future */
static struct hashmap_entry_init_fut
hashmap_entry_init(struct hashmap *hm, struct hashmap_entry *hme, uint64_t key,
		size_t size)
{
	struct hashmap_entry_init_fut future;
	/* Set input values */
	future.data.hm = hm;
	future.data.hme = hme;
	future.data.key = key;
	future.data.size = size;
	/* Set default output value */
	future.output.hme = NULL;

	FUTURE_INIT(&future, hashmap_entry_init_impl);

	return future;
}
/*
 * END of hashmap_entry_init_fut future
 */

/*
 * BEGIN of hashmap_put_fut future
 */
struct hashmap_put_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_init_fut, init_entry);
	FUTURE_CHAIN_ENTRY(struct vdm_operation_future, memcpy_value);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_state);
};

struct hashmap_put_output {
	char *value;
};

FUTURE(hashmap_put_fut, struct hashmap_put_data, struct hashmap_put_output);

/* Maps the 'lock_entry' future data to the 'entry_init' future data */
static void
lookup_lock_entry_to_entry_init_map(
		struct future_context *lookup_lock_entry_ctx,
		struct future_context *init_entry_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_output *lookup_lock_output =
			future_context_get_output(lookup_lock_entry_ctx);
	struct hashmap_entry_init_data *entry_init_data =
			future_context_get_data(init_entry_ctx);
	struct hashmap_entry *hme = lookup_lock_output->hme;

	if (hme == NULL) {
		/*
		 * 'lookup_lock' future failed to find a hashmap entry,
		 * 'entry_init' future shouldn't be executed.
		 */
		init_entry_ctx->state = FUTURE_STATE_COMPLETE;
	}

	entry_init_data->hme = hme;
}

/*
 * Maps the 'entry_init' future entry output data to the 'memcpy_value' future
 * entry data.
 */
static void
entry_init_to_memcpy_value_map(struct future_context *init_entry_ctx,
		    struct future_context *memcpy_value_ctx, void *arg)
{
	struct hashmap_entry_init_output *entry_init_output =
			future_context_get_output(init_entry_ctx);
	struct vdm_operation_data *memcpy_value_data =
			future_context_get_data(memcpy_value_ctx);

	struct hashmap_entry *hme = entry_init_output->hme;
	if (hme == NULL || hme->value.addr == NULL) {
		/*
		 * Either 'lookup_lock' future entry failed to find a hashmap
		 * entry or 'entry_init' future entry failed to initialize it.
		 * 'memcpy_value' future entry shouldn't be executed.
		 */
		memcpy_value_ctx->state = FUTURE_STATE_COMPLETE;
		return;
	}

	struct vdm_operation *memcpy_op = &memcpy_value_data->operation;
	memcpy_op->data.memcpy.dest = hme->value.addr;
}

/*
 * Maps the 'set_state' future entry data to the 'hashmap_put_fut' chained
 * future output data.
 */
static void
set_entry_state_to_output_map_for_put(
		struct future_context *entry_set_state_ctx,
		struct future_context *put_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *entry_set_state_data =
			future_context_get_data(entry_set_state_ctx);
	struct hashmap_put_output *put_output =
			future_context_get_output(put_ctx);
	struct hashmap_entry *hme = entry_set_state_data->hme;

	/*
	 * Map the address of the hashmap locked by the 'lookup_lock_entry',
	 * 'init_entry', 'memcpy_value', and 'set_state' future entries to
	 * the 'hashmap_put' chained future output.
	 */
	put_output->value = (hme) ? hme->value.addr : NULL;
}

/*
 * Lazy initialization function. Initializes the 'set_state' future based on
 * the 'init_entry' future entry result.
 */
static void
set_state_init_for_put(void *future,
		struct future_context *hashmap_put_ctx, void *arg)
{
	struct hashmap_put_data *data =
			future_context_get_data(hashmap_put_ctx);
	struct hashmap_entry_set_state_fut fut = {.output.changed = 0};
	struct hashmap_entry *hme = data->init_entry.fut.output.hme;

	/*
	 * 'lookup_lock_entry' future entry failed to find and lock a hashmap
	 * entry. 'set_state' future entry shouldn't be executed.
	 */
	if (hme == NULL) {
		FUTURE_INIT_COMPLETE(&fut);
		fut.data.hme = NULL;
		goto copy_fut;
	}

	/*
	 * Hashmap entry state should be set to 'unoccupied' when 'init_entry'
	 * future entry failed or 'present' when it was successful.
	 */
	enum hashmap_entry_state state = (hme->value.addr != NULL) ?
		HASHMAP_ENTRY_STATE_PRESENT : HASHMAP_ENTRY_STATE_UNOCCUPIED;

	fut = hashmap_entry_set_state(hme, HASHMAP_ENTRY_STATE_LOCKED,
			state);

copy_fut:
	memcpy(future, &fut, sizeof(fut));
}

/* Creates and initializes a new hashmap_put_fut future */
static struct hashmap_put_fut
hashmap_put(struct vdm *vdm, struct hashmap *hm, uint64_t key, void *value,
		size_t size)
{
	struct hashmap_put_fut chain;
	/* Initialize chained future entries */
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_UNOCCUPIED),
			lookup_lock_entry_to_entry_init_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.init_entry,
			hashmap_entry_init(hm, NULL, key, size),
			entry_init_to_memcpy_value_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.memcpy_value,
			vdm_memcpy(vdm, NULL, value, size, 0), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_state,
			set_state_init_for_put, NULL,
			set_entry_state_to_output_map_for_put, NULL);
	/* Set default output value */
	chain.output.value = NULL;

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_put_fut future
 */

/*
 * BEGIN of hashmap_entry_fini_fut future
 */
struct hashmap_entry_fini_data {
	struct hashmap *hm;
	struct hashmap_entry *hme;
};

struct hashmap_entry_fini_output {
	struct hashmap_entry *hme;
};

FUTURE(hashmap_entry_fini_fut, struct hashmap_entry_fini_data,
		struct hashmap_entry_fini_output);

/*
 * hashmap_entry_fini_fut future task function. Finalizes a hashmap entry.
 */
static enum future_state
hashmap_entry_fini_impl(struct future_context *ctx,
		struct future_notifier *notifier)
{
	struct hashmap_entry_fini_data *data = future_context_get_data(ctx);
	struct hashmap_entry_fini_output *output =
			future_context_get_output(ctx);
	struct hashmap *hm = data->hm;
	struct hashmap_entry *hme = data->hme;
	output->hme = hme;

	free(hme->value.addr);

	size_t old_val = fetch_and_sub(&hm->length, 1);
	assert(old_val != hm->length);

	/* Avoid unused variable warning */
	(void) old_val;

	return FUTURE_STATE_COMPLETE;
}

/* Creates and initializes a new hashmap_entry_fini_fut future */
static struct hashmap_entry_fini_fut
hashmap_entry_fini(struct hashmap *hm, struct hashmap_entry *hme)
{
	struct hashmap_entry_fini_fut future;
	/* Set input values */
	future.data.hm = hm;
	future.data.hme = hme;
	/* Set default output value */
	future.output.hme = NULL;

	FUTURE_INIT(&future, hashmap_entry_fini_impl);

	return future;
}
/*
 * END of hashmap_entry_fini_fut future
 */

/*
 * BEGIN of hashmap_remove_fut future
 */
struct hashmap_remove_data {
	FUTURE_CHAIN_ENTRY(struct hashmap_lookup_lock_entry_fut,
			lookup_lock_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_fini_fut, fini_entry);
	FUTURE_CHAIN_ENTRY(struct hashmap_entry_set_state_fut, set_state);
};

struct hashmap_remove_output {
	uint64_t key;
};

FUTURE(hashmap_remove_fut, struct hashmap_remove_data,
		struct hashmap_remove_output);

/*
 * Maps the 'lookup_lock_entry' future entry data to the 'fini_entry' future
 * entry data.
 */
static void
lookup_lock_entry_to_fini_entry_map(
		struct future_context *lookup_lock_entry_ctx,
		struct future_context *fini_entry_ctx, void *arg)
{
	struct hashmap_lookup_lock_entry_output *lookup_lock_output =
			future_context_get_output(lookup_lock_entry_ctx);
	struct hashmap_entry_fini_data *fini_data =
			future_context_get_data(fini_entry_ctx);
	struct hashmap_entry *hme = lookup_lock_output->hme;

	if (hme == NULL) {
		/*
		 * 'lookup_lock_entry' future entry failed to find a hashmap
		 * entry. 'fini_entry' future entry shouldn't be executed.
		 */
		fini_entry_ctx->state = FUTURE_STATE_COMPLETE;
	}

	fini_data->hme = hme;
}

/*
 * Maps the 'set_state' future entry data to the 'hashmap_remove_fut' chained
 * future output data.
 */
static void
set_entry_state_to_output_map_for_remove(
		struct future_context *entry_set_state_ctx,
		struct future_context *remove_ctx, void *arg)
{
	struct hashmap_entry_set_state_data *entry_set_state_data =
			future_context_get_data(entry_set_state_ctx);
	struct hashmap_remove_output *remove_output =
			future_context_get_output(remove_ctx);
	struct hashmap_entry *hme = entry_set_state_data->hme;
	remove_output->key = (hme) ? hme->key : 0;
}

/*
 * Lazy initialization function. Initializes the 'set_state' future based on
 * the 'fini_entry' future entry result.
 */
static void
set_state_init_for_remove(void *future,
		struct future_context *hashmap_remove_ctx, void *arg)
{
	struct hashmap_remove_data *data =
			future_context_get_data(hashmap_remove_ctx);
	struct hashmap_entry_set_state_fut fut = {.output.changed = 0};
	struct hashmap_entry *hme = data->fini_entry.fut.output.hme;

	if (hme == NULL) {
		/*
		 * 'lookup_lock_entry' future entry failed to find a hashmap
		 * entry. 'set_state' future entry shouldn't be executed.
		 */
		FUTURE_INIT_COMPLETE(&fut);
		fut.data.hme = NULL;
	} else {
		/*
		 * 'fini_entry' future entry was successful. Initialize
		 * the 'set_state' future entry to change the hashmap entry
		 * state to 'unoccupied'.
		 */
		fut = hashmap_entry_set_state(hme,
				HASHMAP_ENTRY_STATE_LOCKED,
				HASHMAP_ENTRY_STATE_UNOCCUPIED);
	}

	memcpy(future, &fut, sizeof(fut));
}

/* Creates and initializes a new hashmap_remove_fut future */
static struct hashmap_remove_fut
hashmap_remove(struct hashmap *hm, uint64_t key)
{
	struct hashmap_remove_fut chain;
	/* Initialize chained future entries */
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.lookup_lock_entry,
			hashmap_lookup_lock_entry(hm, key,
					HASHMAP_ENTRY_STATE_PRESENT),
			lookup_lock_entry_to_fini_entry_map, NULL);
	FUTURE_CHAIN_ENTRY_INIT(&chain.data.fini_entry,
			hashmap_entry_fini(hm, NULL), NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&chain.data.set_state,
			set_state_init_for_remove, NULL,
			set_entry_state_to_output_map_for_remove, NULL);
	/* Set default output value */
	chain.output.key = 0;

	FUTURE_CHAIN_INIT(&chain);

	return chain;
}
/*
 * END of hashmap_remove_fut future
 */

typedef void (*hashmap_cb)(uint64_t key, void *value, void *arg);

/* Executes callback function for each entry stored in hashmap */
static void
hashmap_foreach(struct hashmap *hm, hashmap_cb cb, void *arg)
{
	uint64_t key;
	void *value;
	for (size_t i = 0; i < hm->capacity; i++) {
		if (hashmap_entry_unoccupied(&hm->entries[i])) {
			continue;
		}

		key = hm->entries[i].key;
		value = hm->entries[i].value.addr;

		cb(key, value, arg);
	}
}

/* Hashmap callback, prints key-value pair */
static void
print_entry(uint64_t key, void *value, void *arg)
{
	printf("key: %" PRIu64 ", value: %s\n", key, (char *)value);
}

int
main(void)
{
	/* Set up the data, create a hashmap instance */
	char val_1[] = "Foo";
	char val_2[] = "Bar";
	char val_3[] = "Fizz";
	char val_4[] = "Buzz";
	char other_val[] = "Coffee";

	struct hashmap *hm = hashmap_new(4);
	if (hm == NULL) {
		printf("failed to allocate a new hashmap.\n");
		return 1;
	}

	/* Create a runtime instance for efficient future polling */
	struct runtime *r = runtime_new();
	if (r == NULL) {
		hashmap_delete(hm);

		printf("failed to allocate a new runtime.\n");
		return 1;
	}

	/* Create a thread mover to be used for data move operations */
	struct data_mover_threads *dmt = data_mover_threads_default();
	if (dmt == NULL) {
		runtime_delete(r);
		hashmap_delete(hm);

		printf("failed to allocate data mover.\n");
		return 1;
	}

	struct vdm *tmover = data_mover_threads_get_vdm(dmt);

	/*
	 * Populate the hashmap. Create four 'hashmap_put_fut' futures and wait
	 * for their completion. 'hashmap_put' future implementation uses data
	 * mover for data copying.
	 */
	struct hashmap_put_fut put_futs[4];
	put_futs[0] = hashmap_put(tmover, hm, 1, val_1, strlen(val_1) + 1);
	put_futs[1] = hashmap_put(tmover, hm, 2, val_2, strlen(val_2) + 1);
	put_futs[2] = hashmap_put(tmover, hm, 3, val_3, strlen(val_3) + 1);
	put_futs[3] = hashmap_put(tmover, hm, 4, val_4, strlen(val_4) + 1);

	WAIT_FUTURES(r, put_futs, 4);

	/*
	 * Successful put operation outputs the stored key. Use 'FUTURE_OUTPUT`
	 * macro to extract each future output and assert that none failed.
	 */
	struct hashmap_put_output *put_output;
	for (int i = 0; i < 4; i++) {
		put_output = FUTURE_OUTPUT(&put_futs[i]);
		assert(put_output->value != NULL);
	}
	/*
	 * At this moment hashmap 'hm' stores four entries with the following
	 * key-value pairs: (1, "Foo"), (2, "Bar"), (3, "Fizz"), (4, "Buzz").
	 */

	/* Insert another entry into the hashmap, exceeding hashmap capacity */
	put_futs[0] = hashmap_put(tmover, hm, 404, other_val,
			strlen(other_val) + 1);

	WAIT_FUTURES(r, put_futs, 1);

	/* Failed insert outputs '0' */
	put_output = FUTURE_OUTPUT(&put_futs[0]);
	assert(put_output->value == NULL);

	/*
	 * Make space in the hashmap. Create two 'hashmap_remove_fut` futures
	 * and wait for their completion.
	 */
	struct hashmap_remove_fut remove_futs[2];
	remove_futs[0] = hashmap_remove(hm, 2);
	remove_futs[1] = hashmap_remove(hm, 3);

	WAIT_FUTURES(r, remove_futs, 2);

	/* Successful remove operation outputs key of the removed entry */
	struct hashmap_remove_output *remove_output;
	for (int i = 0; i < 2; i++) {
		remove_output = FUTURE_OUTPUT(&remove_futs[i]);
		assert(remove_output->key != 0);
	}
	/*
	 * Currently, hashmap 'hm' stores two entries with the following
	 * key-value pairs: (1, "Foo"), (4, "Buzz").
	 */

	/* Insert two entries with keys already present in the hashmap */
	put_futs[0] = hashmap_put(tmover, hm, 1, other_val,
			strlen(other_val) + 1);
	put_futs[1] = hashmap_put(tmover, hm, 4, other_val,
			strlen(other_val) + 1);

	WAIT_FUTURES(r, put_futs, 2);

	/* Hashmap cannot store entry with duplicated key */
	for (int i = 0; i < 2; i++) {
		put_output = FUTURE_OUTPUT(&put_futs[i]);
		assert(put_output->value == NULL);
	}

	/*
	 * Get value of the entry with '4' key. Create a 'hashmap_get_fut'
	 * future and wait for its execution.
	 */
	size_t buf_size = 32;
	char *buf = malloc(buf_size);

	struct hashmap_get_copy_fut get_futs[1];
	get_futs[0] = hashmap_get_copy(tmover, hm, 4, buf, buf_size);

	WAIT_FUTURES(r, get_futs, 1);

	/* Entry with '4' key should store value 'Buzz' */
	struct hashmap_get_copy_output *get_copy_output =
			FUTURE_OUTPUT(&get_futs[0]);
	assert(strcmp(buf, val_4) == 0);
	assert(get_copy_output->value == buf);
	assert(get_copy_output->size == strlen(val_4) + 1);
	/* 'hashmap_get_copy_fut' will not copy more data than buffer can fit */
	assert(get_copy_output->copy_size == strlen(val_4) + 1);
	printf("copied value: %s\n", buf);
	free(buf);

	/* Print key-value pairs of every entry stored in the hashmap */
	hashmap_foreach(hm, print_entry, NULL);

	runtime_delete(r);
	hashmap_delete(hm);
	/* Avoid unused variable warning */
	(void) put_output;
	(void) remove_output;
	(void) get_copy_output;

	return 0;
}
