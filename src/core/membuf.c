// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * membuf.c -- membuf implementation
 */

#include "membuf.h"
#include "os_thread.h"
#include "out.h"

#define MEMBUF_ALIGNMENT (1 << 21) /* 2MB */
#define MEMBUF_LEN (1 << 21) /* 2MB */

struct threadbuf {
	struct threadbuf *next; /* next threadbuf */
	struct threadbuf *unused_next; /* next unused threadbuf */

	struct membuf *membuf;

	void *user_data; /* user-specified pointer */
	size_t size; /* size of the buf variable */
	size_t offset; /* current allocation offset */
	size_t available; /* free space available in front of the offset */
	size_t leftovers; /* space left unused on wraparound */
	char buf[]; /* buffer with data */
};

struct membuf {
	os_mutex_t lists_lock; /* protects both lists */
	struct threadbuf *tbuf_first; /* linked-list of threadbufs, cleanup */
	struct threadbuf *tbuf_unused_first; /* list of threadbufs for reuse */

	os_tls_key_t bufkey; /* TLS key for threadbuf */
	void *user_data; /* user-provided buffer data */
};

struct membuf_entry {
	int32_t allocated; /* 1 - allocated, 0 - unused */
	uint32_t size; /* size of the entry */
	char data[]; /* user data */
};

/*
 * membuf_key_destructor -- thread destructor for threadbuf
 */
static void
membuf_key_destructor(void *data)
{
	/*
	 * Linux TLS calls this callback only when a thread exits, but
	 * the Windows FLS implementation also calls it when the key itself
	 * is destroyed. To handle this difference, membuf only actually
	 * deallocates thread buffers on module delete and this callback
	 * puts the now unused thread buffer on a list to be reused.
	 */
	struct threadbuf *tbuf = data;
	struct membuf *membuf = tbuf->membuf;

	os_mutex_lock(&membuf->lists_lock);
	tbuf->unused_next = membuf->tbuf_unused_first;
	membuf->tbuf_unused_first = tbuf;
	os_mutex_unlock(&membuf->lists_lock);
}

/*
 * membuf_new -- allocates and initializes a new membuf instance
 */
struct membuf *
membuf_new(void *user_data)
{
	struct membuf *membuf = malloc(sizeof(struct membuf));
	if (membuf == NULL)
		return NULL;

	membuf->user_data = user_data;
	membuf->tbuf_first = NULL;
	membuf->tbuf_unused_first = NULL;
	os_mutex_init(&membuf->lists_lock);

	os_tls_key_create(&membuf->bufkey, membuf_key_destructor);

	return membuf;
}

/*
 * membuf_delete -- deallocates and cleans up a membuf instance
 */
void
membuf_delete(struct membuf *membuf)
{
	os_tls_key_delete(membuf->bufkey);
	for (struct threadbuf *tbuf = membuf->tbuf_first; tbuf != NULL; ) {
		struct threadbuf *next = tbuf->next;
		util_aligned_free(tbuf);
		tbuf = next;
	}
	os_mutex_destroy(&membuf->lists_lock);
	free(membuf);
}

/*
 * membuf_entry_get_size -- returns the size of an entry
 */
static size_t
membuf_entry_get_size(void *real_ptr)
{
	struct membuf_entry *entry = real_ptr;

	uint32_t size;
	util_atomic_load_explicit32(&entry->size, &size, memory_order_acquire);

	return size;
}

/*
 * membuf_entry_is_allocated -- checks whether the entry is allocated
 */
static int
membuf_entry_is_allocated(void *real_ptr)
{
	struct membuf_entry *entry = real_ptr;
	int32_t allocated;
	util_atomic_load_explicit32(&entry->allocated,
		&allocated, memory_order_acquire);

	return allocated;
}

/*
 * membuf_get_threadbuf -- returns thread-local buffer for allocations
 */
static struct threadbuf *
membuf_get_threadbuf(struct membuf *membuf)
{
	struct threadbuf *tbuf = os_tls_get(membuf->bufkey);
	if (tbuf != NULL)
		return tbuf;

	os_mutex_lock(&membuf->lists_lock);

	if (membuf->tbuf_unused_first != NULL) {
		tbuf = membuf->tbuf_unused_first;
		membuf->tbuf_unused_first = tbuf->unused_next;
	} else {
		/*
		 * Make sure buffer is aligned to 2MB so that we can align down
		 * from contained pointers to access metadata (like user_data).
		 */
		tbuf = util_aligned_malloc(MEMBUF_ALIGNMENT, MEMBUF_LEN);
		if (tbuf == NULL) {
			os_mutex_unlock(&membuf->lists_lock);
			return NULL;
		}

		tbuf->next = membuf->tbuf_first;
		membuf->tbuf_first = tbuf;
	}

	tbuf->size = MEMBUF_LEN - sizeof(*tbuf);
	tbuf->offset = 0;
	tbuf->leftovers = 0;
	tbuf->unused_next = NULL;
	tbuf->membuf = membuf;
	tbuf->available = tbuf->size;
	tbuf->user_data = membuf->user_data;
	os_tls_set(membuf->bufkey, tbuf);

	os_mutex_unlock(&membuf->lists_lock);

	return tbuf;
}

/*
 * membuf_threadbuf_prune -- reclaims available buffer space
 */
static void
membuf_threadbuf_prune(struct threadbuf *tbuf)
{
	while (tbuf->available != tbuf->size) {
		/* reuse leftovers after a wraparound */
		if (tbuf->leftovers != 0 &&
			(tbuf->size - (tbuf->offset + tbuf->available))
				== tbuf->leftovers) {
			tbuf->available += tbuf->leftovers;
			tbuf->leftovers = 0;

			continue;
		}

		/* check the next object after the available memory */
		size_t next_loc = (tbuf->offset + tbuf->available) % tbuf->size;
		void *next = &tbuf->buf[next_loc];
		if (membuf_entry_is_allocated(next))
			return;

		tbuf->available += membuf_entry_get_size(next);
	}
}

/*
 * membuf_alloc -- allocate linearly from the available memory location.
 */
void *
membuf_alloc(struct membuf *membuf, size_t size)
{
	struct threadbuf *tbuf = membuf_get_threadbuf(membuf);
	if (tbuf == NULL)
		return NULL;

	size_t real_size = size + sizeof(struct membuf_entry);

	if (real_size > tbuf->size)
		return NULL;

	if (tbuf->offset + real_size > tbuf->size) {
		tbuf->leftovers = tbuf->available;
		tbuf->offset = 0;
		tbuf->available = 0;
	}

	/* wait until enough memory becomes available */
	if (real_size > tbuf->available) {
		membuf_threadbuf_prune(tbuf);
		/*
		 * Fail if not enough space was reclaimed and no
		 * memory is available for further reclamation.
		 */
		if (real_size > tbuf->available)
			return NULL;
	}

	size_t pos = tbuf->offset;
	tbuf->offset += real_size;
	tbuf->available -= real_size;

	struct membuf_entry *entry = (struct membuf_entry *)&tbuf->buf[pos];
	entry->size = (uint32_t)real_size;
	entry->allocated = 1;

	return &entry->data;
}

/*
 * membuf_free -- deallocates an entry
 */
void
membuf_free(void *ptr)
{
	struct membuf_entry *entry = (struct membuf_entry *)
		((uintptr_t)ptr - sizeof(struct membuf_entry));

	util_atomic_store_explicit64(&entry->allocated, 0,
		memory_order_release);
}

/*
 * membuf_ptr_user_data -- return the user data pointer for membuf associated
 * with the given allocation.
 */
void *
membuf_ptr_user_data(void *ptr)
{
	struct threadbuf *tbuf = (struct threadbuf *)ALIGN_DOWN((ptrdiff_t)ptr,
		MEMBUF_ALIGNMENT);

	return tbuf->user_data;
}
