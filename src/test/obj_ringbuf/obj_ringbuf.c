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
 * obj_ringbuf.c -- unit test for ring buffer
 */
#include <stdint.h>

#include "ringbuf.h"
#include "util.h"
#include "unittest.h"

static void
fill_fetch_all(void)
{
#define RINGBUF_LEN 16

	struct ringbuf *rbuf = ringbuf_new(RINGBUF_LEN);
	UT_ASSERTne(rbuf, NULL);

	for (uint64_t i = 1; i <= RINGBUF_LEN; ++i) {
		ringbuf_enqueue(rbuf, (void *)i);
	}

	UT_ASSERTne(ringbuf_tryenqueue(rbuf, (void *)1), 0);

	for (uint64_t i = 1; i <= RINGBUF_LEN; ++i) {
		void *data = ringbuf_dequeue(rbuf);
		UT_ASSERTeq(data, (void *)i);
	}
	UT_ASSERTeq(ringbuf_trydequeue(rbuf), NULL);

	ringbuf_delete(rbuf);

#undef RINGBUF_LEN
}

struct th_msg {
	int th_id;
	int msg_id;
	int consumed;
};

struct th_arg {
	/* unique */
	int th_id;
	int nmsg;
	struct th_msg *msgs;

	/* shared */
	int nconsumers;
	int nproducers;
	long long *consumers_msg_sum;
	int *msg_per_producer_sum;
	struct ringbuf *rbuf;
};

static void *
producer(void *arg)
{
	struct th_arg *thp = arg;

	struct th_msg *m;
	for (int i = 0; i < thp->nmsg; ++i) {
		m = &thp->msgs[i];
		m->th_id = thp->th_id;
		m->msg_id = i;
		UT_ASSERTeq(m->consumed, 0);

		ringbuf_enqueue(thp->rbuf, m);
	}

	return NULL;
}

static void *
consumer(void *arg)
{
	struct th_arg *thp = arg;

	struct th_msg *m;
	int *last_msg_id = MALLOC(sizeof(int) * thp->nproducers);
	for (int i = 0; i < thp->nproducers; ++i)
		last_msg_id[i] = -1;

	for (int i = 0; i < thp->nmsg; ++i) {
		m = ringbuf_dequeue_s(thp->rbuf, sizeof(struct th_msg));
		long long nmsg_consumed = util_fetch_and_add(
			&thp->msg_per_producer_sum[m->th_id], 1);

		util_fetch_and_add(&m->consumed, 1);

		/* check if the ringbuf is FIFO for a single consumer */
		if (thp->nconsumers == 1) {
			UT_ASSERTeq(last_msg_id[m->th_id], m->msg_id - 1);
			last_msg_id[m->th_id] = m->msg_id;
		}

		util_fetch_and_add(thp->consumers_msg_sum, m->msg_id);

		/*
		 * For multiple consumers, it's guaranteed that each dequeue
		 * will return an element that's at most N before the actual
		 * head at the moment of the call, N is the number of concurrent
		 * consumers.
		 *
		 * The check for this is inherently racey and should be
		 * removed/relaxed if the ASSERT fails.
		 */
		UT_ASSERT(nmsg_consumed - (thp->nproducers / 2) <= m->msg_id ||
			nmsg_consumed + (thp->nproducers / 2) >= m->msg_id);
	}

	FREE(last_msg_id);

	return NULL;
}

static void
many_consumers_many_producers(int nconsumers, int nproducers, int msg_total)
{
#define RINGBUF_LEN 256
	os_thread_t *consumers = MALLOC(sizeof(os_thread_t) * nconsumers);
	os_thread_t *producers = MALLOC(sizeof(os_thread_t) * nproducers);
	long long consumers_msg_sum = 0;
	int *msg_per_producer_sum = ZALLOC(sizeof(int) * nproducers);
	struct th_arg arg_proto = {
		.th_id = 0,
		.nmsg = 0,
		.msgs = NULL,
		.nconsumers = nconsumers,
		.nproducers = nproducers,
		.rbuf = ringbuf_new(RINGBUF_LEN),
		.consumers_msg_sum = &consumers_msg_sum,
		.msg_per_producer_sum = msg_per_producer_sum,
	};
	UT_ASSERTne(arg_proto.rbuf, NULL);

	int msg_per_producer = msg_total / nproducers;
	int msg_per_consumer = msg_total / nconsumers;

	struct th_arg *targ;
	struct th_arg **producer_args =
		MALLOC(sizeof(struct th_arg *) * nproducers);
	struct th_arg **consumer_args =
		MALLOC(sizeof(struct th_arg *) * nconsumers);

	UT_ASSERTeq(msg_total % nproducers, 0);
	UT_ASSERTeq(msg_total % nconsumers, 0);

	for (int i = 0; i < nconsumers; ++i) {
		targ = MALLOC(sizeof(*targ));
		*targ = arg_proto;
		targ->th_id = i;
		targ->nmsg = msg_per_consumer;

		consumer_args[i] = targ;
		PTHREAD_CREATE(&consumers[i], NULL, consumer, targ);
	}

	for (int i = 0; i < nproducers; ++i) {
		targ = MALLOC(sizeof(*targ));
		*targ = arg_proto;
		targ->th_id = i;
		targ->nmsg = msg_per_producer;
		targ->msgs = ZALLOC(sizeof(struct th_msg) * msg_per_producer);

		producer_args[i] = targ;
		PTHREAD_CREATE(&producers[i], NULL, producer, targ);
	}

	for (int i = 0; i < nproducers; ++i)
		PTHREAD_JOIN(producers[i], NULL);

	for (int i = 0; i < nconsumers; ++i)
		PTHREAD_JOIN(consumers[i], NULL);

	long long expected_sum = 0;
	for (int i = 0; i < nproducers; ++i) {
		for (int j = 0; j < msg_per_producer; ++j) {
			expected_sum += j;
			UT_ASSERTeq(producer_args[i]->msgs[j].consumed, 1);
		}
		FREE(producer_args[i]->msgs);
		FREE(producer_args[i]);
	}

	for (int i = 0; i < nconsumers; ++i) {
		FREE(consumer_args[i]);
	}

	UT_ASSERTeq(consumers_msg_sum, expected_sum);

	ringbuf_delete(arg_proto.rbuf);
	FREE(producer_args);
	FREE(consumer_args);
	FREE(msg_per_producer_sum);
	FREE(consumers);
	FREE(producers);
#undef RINGBUF_LEN
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ringbuf");

	fill_fetch_all();

	many_consumers_many_producers(1, 1, 1000000);
	many_consumers_many_producers(1, 10, 1000000);
	many_consumers_many_producers(10, 1, 1000000);
	many_consumers_many_producers(10, 10, 1000000);

	DONE(NULL);
}
