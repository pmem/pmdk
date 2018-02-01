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
 * topic.c - topic implementation
 *
 * This is the transient collection of queues. Topic acts as intermediary
 * between publisher and subscribers. It takes a pending message, turns it
 * into a fully persisted one, and pushes it out to all interested queues.
 */

#include <pthread.h>
#include <stdlib.h>
#include <assert.h>
#include "topic.h"

#define TOPIC_PENDING_MAX 1024
#define TOPIC_QUEUE_MAX 1024

struct topic {
	PMEMobjpool *pop;

	char name[TOPIC_NAME_MAX]; /* topic identifier */
	int running;

	/* lock/cond pair for pending messages array */
	pthread_mutex_t lock;
	pthread_cond_t cond;
	pthread_t worker; /* thread for processing of message_pending */

	/* messages waiting to be persisted and sent out to queues */
	struct message_pending *pending[TOPIC_PENDING_MAX];
	size_t npending;

	/* collection of associated queues */
	struct queue *queue[TOPIC_QUEUE_MAX];
	size_t nqueue;

	struct event_base *base; /* main broker event base, used for closing */
};

/*
 * topic_worker -- processes pending messages
 *
 * This thread waits for messages, persists them and pushes them out to all
 * registered queues in the topic.
 */
static void *
topic_worker(void *arg)
{
	struct topic *t = arg;

	t->running = 1;
	struct message_pending *p[POBJ_MAX_ACTIONS];
	size_t nmsg;

	while (__atomic_load_n(&t->running, __ATOMIC_ACQUIRE)) {
		pthread_mutex_lock(&t->lock);
		while (t->npending == 0) {
			pthread_cond_wait(&t->cond, &t->lock);

			if (!__atomic_load_n(&t->running, __ATOMIC_ACQUIRE))
				goto out;
		}

		/* copy over only up to POBJ_MAX_ACTIONS messages */
		nmsg = t->npending > POBJ_MAX_ACTIONS ?
			POBJ_MAX_ACTIONS : t->npending;
		size_t toff = (size_t)(t->npending - nmsg);
		for (size_t i = 0; i < nmsg; ++i)
			p[i] = t->pending[toff + i];

		t->npending -= nmsg;
		pthread_mutex_unlock(&t->lock);

		/* persist and publish all messages */
		message_pending_publish(t->pop, p, nmsg);

		/* push the message to all registered queues */
		for (int i = 0; i < t->nqueue; ++i) {
			queue_push(t->queue[i], t->pop, p, nmsg);
		}

		/* delete the transient part of the message */
		for (int i = 0; i < nmsg; ++i)
			message_pending_delete(p[i]);
	}

out:
	return NULL;
}

/*
 * topic_recover_queue -- recover the transient state of a queue
 */
static void
topic_recover_queue(struct queue *q, void *arg)
{
	struct topic *topic = arg;

	queue_assign_write_event(q, NULL);
	topic->queue[topic->nqueue++] = q;
}

/*
 * topic_recover_queues -- recover all queues belonging to this topic
 */
static void
topic_recover_queues(struct topic *t)
{
	queue_foreach_in_topic(t->pop, t->name, topic_recover_queue, t);
}

/*
 * topic_new -- creates a new topic instance
 */
struct topic *
topic_new(PMEMobjpool *pop, const char *name, struct event_base *base)
{
	struct topic *t = malloc(sizeof(*t));
	if (t == NULL)
		return NULL;

	t->pop = pop;

	strncpy(t->name, name, TOPIC_NAME_MAX);
	pthread_mutex_init(&t->lock, NULL);
	pthread_cond_init(&t->cond, NULL);

	t->npending = 0;
	t->nqueue = 0;

	t->running = 0;

	t->base = base;

	if (pthread_create(&t->worker, NULL, topic_worker, t) != 0)
		assert(0);

	topic_recover_queues(t);

	return t;
}

/*
 * topic_stop -- signals the worker to stop and breaks the main event loop
 */
void
topic_stop(struct topic *topic)
{
	if (__sync_bool_compare_and_swap(&topic->running, 1, 0)) {
		pthread_mutex_lock(&topic->lock);
		pthread_cond_signal(&topic->cond);
		pthread_mutex_unlock(&topic->lock);
		event_base_loopbreak(topic->base);
	}
}

/*
 * topic_delete -- deletes the topic instance
 */
void
topic_delete(struct topic *topic)
{
	pthread_join(topic->worker, NULL);
	pthread_cond_destroy(&topic->cond);
	pthread_mutex_destroy(&topic->lock);
	free(topic);
}

/*
 * topic_message_schedule -- appends the pending message to the array and
 *	signals the worker thread
 */
int
topic_message_schedule(struct topic *topic, struct message_pending *msg)
{
	pthread_mutex_lock(&topic->lock);

	topic->pending[topic->npending++] = msg;

	pthread_cond_signal(&topic->cond);
	pthread_mutex_unlock(&topic->lock);

	return 0;
}

/*
 * topic_find_create_queue -- searches for a queue with the given name in the
 *	topic, if none exists, creates it
 */
struct queue *
topic_find_create_queue(struct topic *topic, const char *name)
{
	struct queue *q = NULL;
	pthread_mutex_lock(&topic->lock);

	int i;
	for (i = 0; i < topic->nqueue; ++i) {
		q = topic->queue[i];
		if (strncmp(queue_name(q), name, QUEUE_NAME_MAX) == 0)
			goto out;
	}

	q = queue_new(topic->pop, name, topic->name);
	if (q != NULL)
		topic->queue[topic->nqueue++] = q;

out:
	pthread_mutex_unlock(&topic->lock);
	return q;
}
