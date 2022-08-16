// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2022, Intel Corporation */

/*
 * ringbuf.c -- implementation of a simple multi-producer/multi-consumer (MPMC)
 * ring buffer. It uses atomic instructions for correctness and semaphores for
 * waiting.
 */

/* disable conditional expression is const warning */
#ifdef _WIN32
#pragma warning(disable : 4127)
#endif

#include "core/valgrind_internal.h"

#include "ringbuf.h"
#include "util.h"
#include "out.h"
#include "os.h"
#include "os_thread.h"
#include "sys_util.h"

#ifdef _WIN32
#define __sync_synchronize() MemoryBarrier()
#endif

#if 1
/*
 * This number defines by how much the relevant semaphore will be increased to
 * unlock waiting threads and thus defines how many threads can wait on
 *  the ring buffer at the same time.
 */
#define RINGBUF_MAX_CONSUMER_THREADS 1024
#endif

/* avoid false sharing by padding the variable */
#define CACHELINE_PADDING(type, name)\
union { type name; uint64_t name##_padding[8]; } name##_padded

struct ringbuf {
	CACHELINE_PADDING(uint64_t, read_pos);
	CACHELINE_PADDING(uint64_t, write_pos);

	CACHELINE_PADDING(os_semaphore_t, nfree);
	CACHELINE_PADDING(os_semaphore_t, nused);

	uint64_t len_mask;
	unsigned len;
	int running;

	void *data[];
};

/*
 * ringbuf_new -- creates a new ring buffer instance
 */
struct ringbuf *
ringbuf_new(unsigned length)
{
	LOG(4, NULL);

	/* length must be a power of two due to masking */
	if (util_popcount(length) > 1)
		return NULL;

	struct ringbuf *rbuf =
		calloc(length * sizeof(void *), sizeof(*rbuf));
	if (rbuf == NULL)
		return NULL;

	if (os_semaphore_init(&rbuf->nfree_padded.nfree, length)) {
		free(rbuf);
		return NULL;
	}

	if (os_semaphore_init(&rbuf->nused_padded.nused, 0)) {
		util_semaphore_destroy(&rbuf->nfree_padded.nfree);
		free(rbuf);
		return NULL;
	}

	rbuf->read_pos_padded.read_pos = 0;
	rbuf->write_pos_padded.write_pos = 0;

	rbuf->len = length;
	rbuf->len_mask = length - 1;
	rbuf->running = 1;

	return rbuf;
}

#if 1
/*
 * ringbuf_length -- returns the length of the ring buffer
 */
unsigned
ringbuf_length(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	return rbuf->len;
}

/*
 * ringbuf_stop -- if there are any threads stuck waiting on dequeue, unblocks
 *	them. Those threads, if there are no new elements, will return NULL.
 */
void
ringbuf_stop(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	/* wait for the buffer to become empty */
	while (rbuf->read_pos_padded.read_pos !=
		rbuf->write_pos_padded.write_pos)
		__sync_synchronize();

	int ret = util_bool_compare_and_swap64(&rbuf->running, 1, 0);
	ASSERTeq(ret, 1);

	/* XXX just unlock all waiting threads somehow... */
	for (int64_t i = 0; i < RINGBUF_MAX_CONSUMER_THREADS; ++i)
		util_semaphore_post(&rbuf->nused_padded.nused);
}
#endif

/*
 * ringbuf_delete -- destroys an existing ring buffer instance
 */
void
ringbuf_delete(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	ASSERTeq(rbuf->read_pos_padded.read_pos,
		rbuf->write_pos_padded.write_pos);
	util_semaphore_destroy(&rbuf->nfree_padded.nfree);
	util_semaphore_destroy(&rbuf->nused_padded.nused);
	free(rbuf);
}

/*
 * ringbuf_enqueue_atomic -- (internal) performs the lockfree insert of
 * an element into the ringbuf data array
 */
static void
ringbuf_enqueue_atomic(struct ringbuf *rbuf, void *data)
{
	LOG(4, NULL);

	size_t w = util_fetch_and_add64(&rbuf->write_pos_padded.write_pos, 1)
		& rbuf->len_mask;

	ASSERT(rbuf->running);

	/*
	 * In most cases, this won't loop even once, but sometimes if
	 * the semaphore is incremented concurrently in dequeue,
	 * we need to wait.
	 */
	while (!util_bool_compare_and_swap64(&rbuf->data[w], NULL, data))
		;

	VALGRIND_ANNOTATE_HAPPENS_BEFORE(&rbuf->data[w]);
}

#if 1
/*
 * ringbuf_enqueue -- places a new value into the collection
 *
 * This function blocks if there's no space in the buffer.
 */
int
ringbuf_enqueue(struct ringbuf *rbuf, void *data)
{
	LOG(4, NULL);

	util_semaphore_wait(&rbuf->nfree_padded.nfree);

	ringbuf_enqueue_atomic(rbuf, data);

	util_semaphore_post(&rbuf->nused_padded.nused);

	return 0;
}
#endif

/*
 * ringbuf_tryenqueue -- places a new value into the collection
 *
 * This function fails if there's no space in the buffer.
 */
int
ringbuf_tryenqueue(struct ringbuf *rbuf, void *data)
{
	LOG(4, NULL);

	if (util_semaphore_trywait(&rbuf->nfree_padded.nfree) != 0)
		return -1;

	ringbuf_enqueue_atomic(rbuf, data);

	util_semaphore_post(&rbuf->nused_padded.nused);

	return 0;
}

/*
 * ringbuf_dequeue_atomic -- performs a lockfree retrieval of data from ringbuf
 */
static void *
ringbuf_dequeue_atomic(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	size_t r = util_fetch_and_add64(&rbuf->read_pos_padded.read_pos, 1)
		& rbuf->len_mask;
	/*
	 * Again, in most cases, there won't be even a single loop, but if one
	 * thread stalls while others perform work, it might happen that two
	 * threads get the same read position.
	 */
	void *data = NULL;

	VALGRIND_ANNOTATE_HAPPENS_AFTER(&rbuf->data[r]);
	do {
		do {
			util_atomic_load64(&rbuf->data[r], &data);
		} while (data == NULL);
	} while (!util_bool_compare_and_swap64(&rbuf->data[r], data, NULL));

	return data;
}

#if 1
/*
 * ringbuf_dequeue -- retrieves one value from the collection
 *
 * This function blocks if there are no values in the buffer.
 */
void *
ringbuf_dequeue(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	util_semaphore_wait(&rbuf->nused_padded.nused);

	if (!rbuf->running)
		return NULL;

	void *data = ringbuf_dequeue_atomic(rbuf);

	util_semaphore_post(&rbuf->nfree_padded.nfree);

	return data;
}
#endif

/*
 * ringbuf_trydequeue -- retrieves one value from the collection
 *
 * This function fails if there are no values in the buffer.
 */
void *
ringbuf_trydequeue(struct ringbuf *rbuf)
{
	LOG(4, NULL);

	if (util_semaphore_trywait(&rbuf->nused_padded.nused) != 0)
		return NULL;

	if (!rbuf->running)
		return NULL;

	void *data = ringbuf_dequeue_atomic(rbuf);

	util_semaphore_post(&rbuf->nfree_padded.nfree);

	return data;
}

/*
 * ringbuf_trydequeue_s -- valgrind-safe variant of the trydequeue function
 *
 * This function is needed for runtime race detection as a way to avoid false
 * positives due to usage of atomic instructions that might otherwise confuse
 * valgrind.
 */
void *
ringbuf_trydequeue_s(struct ringbuf *rbuf, size_t data_size)
{
	LOG(4, NULL);

	void *r = ringbuf_trydequeue(rbuf);
	if (r != NULL)
		VALGRIND_ANNOTATE_NEW_MEMORY(r, data_size);

	return r;
}

#if 0
/*
 * ringbuf_dequeue_s -- valgrind-safe variant of the dequeue function
 *
 * This function is needed for runtime race detection as a way to avoid false
 * positives due to usage of atomic instructions that might otherwise confuse
 * valgrind.
 */
void *
ringbuf_dequeue_s(struct ringbuf *rbuf, size_t data_size)
{
	LOG(4, NULL);

	void *r = ringbuf_dequeue(rbuf);
	VALGRIND_ANNOTATE_NEW_MEMORY(r, data_size);

	return r;
}
#endif
