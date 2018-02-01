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
 * queue.h - message queue interface
 */

#include <libpmemobj.h>
#include <stddef.h>
#include <event2/event.h>
#include "message.h"

#define QUEUE_NAME_MAX 8
TOID_DECLARE(struct queue, 101);

struct queue;

void queue_foreach_in_topic(PMEMobjpool *pop, const char *topic,
	void (*cb)(struct queue *, void *arg), void *arg);
struct queue *queue_new(PMEMobjpool *pop, const char *name, const char *topic);

const char *queue_name(struct queue *queue);

int queue_empty(struct queue *queue);

int queue_assign_write_event(struct queue *queue, struct event *e);

int queue_push(struct queue *queue, PMEMobjpool *pop,
	struct message_pending **pending, size_t npending);

TOID(struct message) queue_peek(struct queue *queue);
void queue_pop(struct queue *queue);

void queue_recover_all(PMEMobjpool *pop);
