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
 * message.c - message implementation
 */

#include <libpmemobj.h>
#include <stdlib.h>
#include "message.h"

struct message { /* persistent part of the message */
	/*
	 * Reference count used to decide whether or not the message has been
	 * sent out from all subscribing queues.
	 * Message after publishing has refc equal 0, and it is increased for
	 * every queue to which this message is added. Once the message is
	 * sent out and removed from the queue, the reference count is decreased
	 * and the object is eventually freed.
	 * Because of multithreaded nature of the broker, this variable needs to
	 * be manipulated using atomic operations outside of a persistent
	 * transaction. For this reason the refc variable is transient and there
	 * is a recovery process that walks over all the queues and calculates
	 * on how many queues this message is present.
	 */
	int refc;

	size_t len; /* length of data buffer */
	char data[]; /* payload */
};

struct message_pending { /* transient part of the message */
	struct pobj_action act; /* publishable action */
	TOID(struct message) msg; /* persistent message */
};

/*
 * message_new -- creates a new transient message
 */
struct message_pending *
message_new(PMEMobjpool *pop, size_t size)
{
	struct message_pending *p = malloc(sizeof(*p));
	if (p == NULL)
		goto err;

	/* reserve a buffer large enough to fit the entire payload */
	p->msg = POBJ_RESERVE_ALLOC(pop, struct message,
		sizeof(struct message) + size, &p->act);
	if (TOID_IS_NULL(p->msg))
		goto err_reserve;

	D_RW(p->msg)->len = size;
	D_RW(p->msg)->refc = 0;

	return p;

err_reserve:
	free(p);
err:
	return NULL;
}

/*
 * message_get -- returns the persistent message from pending
 */
TOID(struct message)
message_get(struct message_pending *pending)
{
	return pending->msg;
}

/*
 * message_ref -- bumps refrence count of the message
 */
void
message_ref(TOID(struct message) msg)
{
	__sync_fetch_and_add(&D_RW(msg)->refc, 1);
}

/*
 * message_unref -- decreases refrence count of the message, frees it if 0
 */
void
message_unref(TOID(struct message) msg)
{
	if (__sync_sub_and_fetch(&D_RW(msg)->refc, 1) == 0)
		POBJ_FREE(&msg);
}

/*
 * message_data -- returns the message's data buffer
 */
void *
message_data(TOID(struct message) msg)
{
	return D_RW(msg)->data;
}

/*
 * message_length -- returns the length of the message's data buffer
 */
size_t
message_length(TOID(struct message) msg)
{
	return D_RO(msg)->len;
}

/*
 * message_pending_publish -- atomically publishes an array of pending messages
 *
 * This is not a part of a transaction because we rely on reference counting
 * to free any messages that might have been published but not added to any of
 * the queues.
 */
void
message_pending_publish(PMEMobjpool *pop,
	struct message_pending **pending, size_t npending)
{
	struct pobj_action *actv =
		malloc(sizeof(struct pobj_action) * npending);
	if (actv == NULL)
		return;

	for (size_t i = 0; i < npending; ++i) {
		struct message *m = D_RW(pending[i]->msg);
		pmemobj_persist(pop, m, sizeof(*m) + m->len);

		actv[i] = pending[i]->act;
	}

	pmemobj_publish(pop, actv, (int)npending);

	free(actv);
}

/*
 * message_pending_delete -- deletes the transient pending message
 *
 * The underlying persistent message is left untouched.
 */
void
message_pending_delete(struct message_pending *pending)
{
	free(pending);
}

/*
 * message_clear_refc_all -- zeroes the reference count of all the messages in
 *	the pool
 */
void
message_clear_refc_all(PMEMobjpool *pop)
{
	TOID(struct message) msg;
	POBJ_FOREACH_TYPE(pop, msg) {
		D_RW(msg)->refc = 0;
	}
}

/*
 * message_delete_unref -- deletes all messages in the pool that have reference
 *	count equal 0
 */
void
message_delete_unref(PMEMobjpool *pop)
{
	TOID(struct message) msg;
	TOID(struct message) nmsg;

	POBJ_FOREACH_SAFE_TYPE(pop, msg, nmsg) {
		if (D_RO(msg)->refc == 0)
			POBJ_FREE(&msg);
	}
}
