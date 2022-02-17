// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * membuf.c -- membuf implementation
 */

#include "membuf.h"
#include "core/os_thread.h"
#include "core/out.h"

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
	membuf_ptr_check check_func; /* object state check function */
	membuf_ptr_size size_func; /* object size function */
	void *func_data; /* user-provided function argument */
	void *user_data; /* user-provided buffer data */
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
membuf_new(membuf_ptr_check check_func, membuf_ptr_size size_func,
	void *func_data, void *user_data)
{
	struct membuf *membuf = malloc(sizeof(struct membuf));
	if (membuf == NULL)
		return NULL;

	membuf->user_data = user_data;
	membuf->check_func = check_func;
	membuf->size_func = size_func;
	membuf->func_data = func_data;
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
		if (tbuf == NULL)
			return NULL;

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
static int
membuf_threadbuf_prune(struct membuf *membuf,
	struct threadbuf *tbuf)
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
		switch (membuf->check_func(next, membuf->func_data)) {
			case MEMBUF_PTR_CAN_REUSE: {
				size_t s = membuf->size_func(next,
					membuf->func_data);
				tbuf->available += s;
			} break;
			case MEMBUF_PTR_CAN_WAIT:
				return 0;
			case MEMBUF_PTR_IN_USE:
				return -1;
		}
	}

	return 0;
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

	if (size > tbuf->size)
		return NULL;

	if (tbuf->offset + size > tbuf->size) {
		tbuf->leftovers = tbuf->available;
		tbuf->offset = 0;
		tbuf->available = 0;
	}

	/* wait until enough memory becomes available */
	while (size > tbuf->available) {
		if (membuf_threadbuf_prune(membuf, tbuf) < 0) {
			/*
			 * Fail if not enough space was reclaimed and no
			 * memory is available for further reclamation.
			 */
			if (size > tbuf->available)
				return NULL;
		}
	}

	size_t pos = tbuf->offset;
	tbuf->offset += size;
	tbuf->available -= size;

	return &tbuf->buf[pos];
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
