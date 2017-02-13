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
#include <semaphore.h>
#include "valgrind_internal.h"

#include "ringbuf.h"
#include "util.h"
#include "out.h"

/* avoid false sharing by padding the variable */
#define CACHELINE_PADDING(type, name)\
union { type name; uint64_t name##_padding[8]; }

struct ringbuf {
	CACHELINE_PADDING(uint64_t, read_pos);
	CACHELINE_PADDING(uint64_t, write_pos);

	CACHELINE_PADDING(sem_t, nfree);
	CACHELINE_PADDING(sem_t, nused);

	uint64_t len;
	uint64_t len_mask;

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

	sem_init(&rbuf->nfree, 0, length);
	sem_init(&rbuf->nused, 0, 0);

	rbuf->read_pos = 0;
	rbuf->write_pos = 0;

	rbuf->len = length;
	rbuf->len_mask = length - 1;

	return rbuf;
}

/*
 * ringbuf_delete -- destroys an existing ring buffer instance
 */
void
ringbuf_delete(struct ringbuf *rbuf)
{
	sem_destroy(&rbuf->nfree);
	sem_destroy(&rbuf->nused);
	Free(rbuf);
}

/*
 * ringbuf_enqueue -- places a new value into the collection
 *
 * This function blocks if there's no space in the buffer.
 */
void
ringbuf_enqueue(struct ringbuf *rbuf, void *data)
{
	sem_wait(&rbuf->nfree);
	size_t w = __sync_fetch_and_add(&rbuf->write_pos, 1) & rbuf->len_mask;

	/*
	 * In most cases, this won't loop even once, but sometimes if the
	 * semaphore is incremented concurrently in dequeue, we need to wait.
	 */
	while (!__sync_bool_compare_and_swap(&rbuf->data[w], NULL, data))
		;

	sem_post(&rbuf->nused);
}

/*
 * ringbuf_dequeue -- retrieves one value from the collection
 *
 * This function blocks if there are no values in the buffer.
 */
void *
ringbuf_dequeue(struct ringbuf *rbuf)
{
	sem_wait(&rbuf->nused);
	size_t r = __sync_fetch_and_add(&rbuf->read_pos, 1) & rbuf->len_mask;

	/*
	 * Again, in most cases, there won't be even a single loop, but if one
	 * thread stalls while others perform work, it might happen that two
	 * threads get the same read position.
	 */
	void *data;
	do {
		while ((data = rbuf->data[r]) == NULL)
			__sync_synchronize();
	} while (!__sync_bool_compare_and_swap(&rbuf->data[r], data, NULL));

	sem_post(&rbuf->nfree);

	return data;
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
	int nused;
	sem_getvalue(&rbuf->nused, &nused);

	return nused == 0;
}

/*
 * ringbuf_full -- returns whether the collection is full
 */
int
ringbuf_full(struct ringbuf *rbuf)
{
	int nfree;
	sem_getvalue(&rbuf->nfree, &nfree);

	return nfree == 0;
}
