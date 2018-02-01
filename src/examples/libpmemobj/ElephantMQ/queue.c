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
 * queue.c - message queue implementation
 *
 * A persistent collection of messages to be sent to a client.
 */

#include <libpmemobj.h>
#include "queue.h"

/* for simplicity sake, the number of messages in a queue is limited */
#define QUEUE_MSG_MAX 1024
#define QUEUE_MSG_MAX_MASK (1024 - 1)

#define QUEUE_TOPIC_MAX 16

struct queue {
	PMEMmutex lock; /* lock protecting this entire structure */

	char name[QUEUE_NAME_MAX]; /* queue identifier */
	char topic[QUEUE_TOPIC_MAX]; /* topic to which this queue belongs */

	/* the array of messages to be sent out */
	size_t front;
	size_t rear;
	TOID(struct message) msg[QUEUE_MSG_MAX];

	struct event *ev_write; /* write event of attached client, transient */
};

/*
 * queue_foreach_in_topic -- calls a callback for each queue belonging to the
 *	given topic.
 *
 * This is used in recovery process to reset the 'ev_write' variable and rebuild
 * the topic's queue collection.
 */
void
queue_foreach_in_topic(PMEMobjpool *pop, const char *topic,
	void (*cb)(struct queue *, void *arg), void *arg)
{
	TOID(struct queue) q;
	POBJ_FOREACH_TYPE(pop, q) {
		if (strncmp(D_RO(q)->topic, topic, QUEUE_TOPIC_MAX) == 0)
			cb(D_RW(q), arg);
	}
}

struct queue_constructor_args {
	const char *name;
	const char *topic;
};

/*
 * queue_constructor -- persistent constructor of a new queue
 */
static int
queue_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct queue *q = ptr;
	struct queue_constructor_args *args = arg;

	memset(q, 0, sizeof(*q));
	strncpy(q->name, args->name, QUEUE_NAME_MAX);
	strncpy(q->topic, args->topic, QUEUE_TOPIC_MAX);
	q->ev_write = NULL;
	q->front = 0;
	q->rear = 0;

	pmemobj_persist(pop, q, sizeof(*q));

	return 0;
}

/*
 * queue_new -- creates a new persistent queue
 *
 * The queues are not linked in any collection other than the implicit one
 * provided by libpmemobj. This is to avoid having persistent topics.
 * A real implementation might consider changing it so that a traversal of
 * queues for recovery is not necessary.
 */
struct queue *
queue_new(PMEMobjpool *pop, const char *name, const char *topic)
{
	struct queue_constructor_args args = {name, topic};

	TOID(struct queue) q;
	POBJ_NEW(pop, &q, struct queue, queue_constructor, &args);

	return D_RW(q);
}

/*
 * queue_empty -- returns whether the queue is empty
 */
int
queue_empty(struct queue *queue)
{
	return queue->rear == queue->front;
}

/*
 * queue_name -- returns the queue's name string
 */
const char *
queue_name(struct queue *queue)
{
	return queue->name;
}

/*
 * queue_push -- adds an array of messages to the queue, if successful and
 *	the queue has an attached client, triggers a write event.
 */
int
queue_push(struct queue *queue, PMEMobjpool *pop,
	struct message_pending **pending, size_t npending)
{
	if (QUEUE_MSG_MAX - (queue->rear - queue->front) < npending)
		return -1;

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &queue->lock) {
		size_t pos = queue->rear & QUEUE_MSG_MAX_MASK;

		/* add all of the necessary array fields upfront */
		pmemobj_tx_add_range_direct(&queue->msg[pos],
			sizeof(TOID(struct message)) * npending);

		for (int i = 0; i < npending; ++i) {
			TOID(struct message) msg = message_get(pending[i]);
			/*
			 * It doesn't matter when the message's ref count is
			 * increased because the recovery step will reset
			 * it regardless of the transaction outcome.
			 */
			message_ref(msg);

			queue->msg[pos] = msg;
		}
		TX_SET_DIRECT(queue, rear, queue->rear + npending);
	} TX_ONCOMMIT {
		if (queue->ev_write != NULL)
			event_add(queue->ev_write, NULL);
	} TX_END

	return 0;
}

/*
 * queue_peek -- returns the current head of the queue
 *
 * Calling thread needs to ensure that nothing will touch the queue's front.
 */
TOID(struct message)
queue_peek(struct queue *queue)
{
	if (queue_empty(queue))
		return TOID_NULL(struct message);

	return queue->msg[queue->front  & QUEUE_MSG_MAX_MASK];
}

/*
 * queue_pop -- removes the head of the queue
 */
void
queue_pop(struct queue *queue)
{
	PMEMobjpool *pop = pmemobj_pool_by_ptr(queue);
	TOID(struct message) c = queue->msg[queue->front & QUEUE_MSG_MAX_MASK];

	TX_BEGIN_PARAM(pop, TX_PARAM_MUTEX, &queue->lock) {
		TX_SET_DIRECT(queue, front, queue->front + 1);
	} TX_END

	message_unref(c);
}

/*
 * queue_assign_write_event -- assigns a client's write event to the queue
 *
 * There can be only one assigned event at a time.
 */
int
queue_assign_write_event(struct queue *queue, struct event *e)
{
	int ret = -1;
	PMEMobjpool *pop = pmemobj_pool_by_ptr(queue);
	pmemobj_mutex_lock(pop, &queue->lock);

	if (queue->ev_write != NULL && e != NULL)
		goto out;

	queue->ev_write = e;
	if (!queue_empty(queue) && queue->ev_write != NULL)
		event_add(queue->ev_write, NULL);

	ret = 0;

out:
	pmemobj_mutex_unlock(pop, &queue->lock);

	return ret;
}

/*
 * queue_ref_messages -- bumps a reference count of every message present
 *	in the queue
 */
static void
queue_ref_messages(struct queue *queue)
{
	for (size_t i = queue->front; i < queue->rear; ++i)
		message_ref(queue->msg[i]);
}

/*
 * queue_recover_all -- recalculate all messages reference count
 */
void
queue_recover_all(PMEMobjpool *pop)
{
	/* 1. zero reference count of all messages in the pool */
	message_clear_refc_all(pop);

	/* 2. for each message in each queue, bump reference count */
	TOID(struct queue) q;
	POBJ_FOREACH_TYPE(pop, q) {
		queue_ref_messages(D_RW(q));
	}

	/* 3. if there are any messages with refc equal 0, free them */
	message_delete_unref(pop);
}
