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
 * ringbuf.c -- implementation of a simple multi-producer/multi-consumer (MPMC)
 * ring buffer. It uses atomic instructions for correctness and semaphores for
 * waiting.
 */

#include <sched.h>
#include "valgrind_internal.h"

#include "ringbuf.h"
#include "util.h"
#include "out.h"
#include "os.h"

/* avoid false sharing by padding the variable */
#define CACHELINE_PADDING(type, name)\
union { type name; uint64_t name##_padding[8]; }

struct ringbuf {
	CACHELINE_PADDING(uint64_t, read_pos);
	CACHELINE_PADDING(uint64_t, write_pos);

	CACHELINE_PADDING(struct os_semaphore *, nfree);
	CACHELINE_PADDING(struct os_semaphore *, nused);

	unsigned len;
	uint64_t len_mask;
	int running;

	void *data[];
};

/*
 * ringbuf_new -- creates a new ring buffer instance
 */
struct ringbuf *
ringbuf_new(unsigned length)
{
	/* length must be a power of two due to masking */
	ASSERT(__builtin_popcount(length) <= 1);

	struct ringbuf *rbuf =
		Zalloc(sizeof(*rbuf) + (length * sizeof(void *)));
	if (rbuf == NULL)
		return NULL;

	rbuf->nfree = os_semaphore_new(length);
	if (rbuf->nfree == NULL)
		goto error;

	rbuf->nused = os_semaphore_new(0);
	if (rbuf->nused == NULL)
		goto error;

	rbuf->read_pos = 0;
	rbuf->write_pos = 0;

	rbuf->len = length;
	rbuf->len_mask = length - 1;
	rbuf->running = 1;

	return rbuf;

error:
	Free(rbuf->nfree);
	Free(rbuf->nused);
	Free(rbuf);

	return NULL;
}

/*
 * ringbuf_length -- returns the length of the ring buffer
 */
unsigned
ringbuf_length(struct ringbuf *rbuf)
{
	return rbuf->len;
}

/*
 * ringbuf_stop -- if there are any threads stuck waiting on dequeue, unblocks
 *	them. Those threads, if there are no new elements, will return NULL.
 */
void
ringbuf_stop(struct ringbuf *rbuf)
{
	rbuf->running = 0;

	/* XXX just unlock all waiting threads somehow... */
	for (uint64_t i = 0; i < 1024; ++i)
		os_semaphore_post(rbuf->nused);
}

/*
 * ringbuf_delete -- destroys an existing ring buffer instance
 */
void
ringbuf_delete(struct ringbuf *rbuf)
{
	os_semaphore_delete(rbuf->nfree);
	os_semaphore_delete(rbuf->nused);
	Free(rbuf);
}

/*
 * ringbuf_enqueue_atomic -- (internal) performs the lockfree insert of an
 *	element into the ringbuf data array
 */
static void
ringbuf_enqueue_atomic(struct ringbuf *rbuf, void *data)
{
	size_t w = __sync_fetch_and_add(&rbuf->write_pos, 1) & rbuf->len_mask;

	ASSERT(rbuf->running);

	/*
	 * In most cases, this won't loop even once, but sometimes if the
	 * semaphore is incremented concurrently in dequeue, we need to wait.
	 */
	while (!__sync_bool_compare_and_swap(&rbuf->data[w], NULL, data))
		;
}

/*
 * ringbuf_enqueue -- places a new value into the collection
 *
 * This function blocks if there's no space in the buffer.
 */
int
ringbuf_enqueue(struct ringbuf *rbuf, void *data)
{
	os_semaphore_wait(rbuf->nfree);

	ringbuf_enqueue_atomic(rbuf, data);

	os_semaphore_post(rbuf->nused);

	return 0;
}

/*
 * ringbuf_tryenqueue -- places a new value into the collection
 *
 * This function fails if there's no space in the buffer.
 */
int
ringbuf_tryenqueue(struct ringbuf *rbuf, void *data)
{
	if (os_semaphore_trywait(rbuf->nfree) != 0)
		return -1;

	ringbuf_enqueue_atomic(rbuf, data);

	os_semaphore_post(rbuf->nused);

	return 0;
}

/*
 * ringbuf_dequeue_atomic -- performs a lockfree retrieval of data from ringbuf
 */
static void *
ringbuf_dequeue_atomic(struct ringbuf *rbuf)
{
	size_t r = __sync_fetch_and_add(&rbuf->read_pos, 1) & rbuf->len_mask;
	/*
	 * Again, in most cases, there won't be even a single loop, but if one
	 * thread stalls while others perform work, it might happen that two
	 * threads get the same read position.
	 */
	void *data = NULL;
	do {
		while ((data = rbuf->data[r]) == NULL && rbuf->running)
			__sync_synchronize();

		if (__sync_bool_compare_and_swap(&rbuf->data[r], data, NULL))
			break;
	} while (rbuf->running);


	return data;
}

/*
 * ringbuf_dequeue -- retrieves one value from the collection
 *
 * This function blocks if there are no values in the buffer.
 */
void *
ringbuf_dequeue(struct ringbuf *rbuf)
{
	os_semaphore_wait(rbuf->nused);

	void *data = ringbuf_dequeue_atomic(rbuf);

	os_semaphore_post(rbuf->nfree);

	return data;
}

/*
 * ringbuf_trydequeue -- retrieves one value from the collection
 *
 * This function fails if there are no values in the buffer.
 */
void *
ringbuf_trydequeue(struct ringbuf *rbuf)
{
	if (os_semaphore_trywait(rbuf->nused) != 0)
		return NULL;

	void *data = ringbuf_dequeue_atomic(rbuf);

	os_semaphore_post(rbuf->nfree);

	return data;
}

/*
 * ringbuf_trydequeue_s -- valgrind-safe variant of the trydequeue function
 */
void *
ringbuf_trydequeue_s(struct ringbuf *rbuf, size_t data_size)
{
	void *r = ringbuf_trydequeue(rbuf);
	if (r != NULL)
		VALGRIND_ANNOTATE_NEW_MEMORY(r, data_size);

	return r;
}

/*
 * ringbuf_dequeue_s -- valgrind-safe variant of the dequeue function
 */
void *
ringbuf_dequeue_s(struct ringbuf *rbuf, size_t data_size)
{
	void *r = ringbuf_dequeue(rbuf);
	VALGRIND_ANNOTATE_NEW_MEMORY(r, data_size);

	return r;
}

/*
 * ringbuf_empty -- returns whether the collection is empty
 */
int
ringbuf_empty(struct ringbuf *rbuf)
{
	return os_semaphore_get(rbuf->nused) == 0;
}

/*
 * ringbuf_full -- returns whether the collection is full
 */
int
ringbuf_full(struct ringbuf *rbuf)
{
	return os_semaphore_get(rbuf->nfree) == 0;
}
