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
 * client.c - broker client implementation
 *
 * This module handles read/write events of individual connections.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <libpmemobj.h>
#include "client.h"
#include "protocol.h"

#define CLIENT_MSG_BUF 128

struct client {
	PMEMobjpool *pop;

	struct topic *topic; /* the default topic for sub/pub actions */

	int fd; /* client socket */
	struct event *ev_read;
	struct event *ev_write;

	char *buf; /* buffer for reads, must either be cmsg or pending->data */
	size_t buf_len; /* length of the above buffer */
	size_t buf_offset; /* current read offset into the buffer */

	char cmsg[CLIENT_MSG_BUF]; /* buffer for normal messages */

	struct message_pending *pending; /* buffer to payload messages */
	size_t write_offset; /* offset for write in pending message */

	struct queue *queue; /* the persistent queue for this client */
};

/*
 * on_message_completion -- called whenever a pending message is complete
 *
 * Moves the message to the topic for persisting and distribution to all
 *	subscribing queues.
 */
static void
on_message_completion(struct client *c)
{
	c->buf = c->cmsg;
	c->buf_offset = 0;
	c->buf_len = CLIENT_MSG_BUF;

	topic_message_schedule(c->topic, c->pending);
	c->pending = NULL;
}

/*
 * cmsg_publish_handler -- handles the PUB client message.
 */
static ssize_t
cmsg_publish_handler(struct client *c, const char *buf, size_t len)
{
	/*
	 * Protocol defines publish as:
	 * PUB <data length>;\n
	 *
	 * Data length is required to create an appropriately sized reservation
	 * ahead of receiving the entire buffer.
	 */
	size_t data_len;
	if (sscanf(buf, "PUB %lu", &data_len) != 1)
		return -1;

	/* create a transient pending message */
	c->pending = message_new(c->pop, data_len);
	if (c->pending == NULL)
		return -1;

	TOID(struct message) m = message_get(c->pending);
	char *nbuf = message_data(m);
	size_t nlen = message_length(m);

	/*
	 * Because data needs to be read even in the absence of a pending
	 * message, and it's impractial to read in single byte buffers, some of
	 * the already read data might need to be copied into the newly
	 * reserved pending message.
	 */
	size_t buflen = ((uintptr_t)c->buf + c->buf_offset) - (uintptr_t)buf;
	buflen -= len;
	size_t overfill = buflen >= nlen ? nlen : buflen;
	memcpy(nbuf, buf + len, overfill);

	/*
	 * It might even happen that for small messages the entire payload is
	 * received in a single read() call.
	 */
	if (overfill == nlen) {
		on_message_completion(c);
		return (ssize_t)(len + overfill); /* move the loop forward */
	}

	c->buf = nbuf;
	c->buf_offset = overfill;
	c->buf_len = nlen;

	return 0;
}

/*
 * cmsg_subscribe_handler -- handles the SUB client message
 *
 * Subscribes to the topic with a named queue.
 */
static ssize_t
cmsg_subscribe_handler(struct client *c, const char *buf, size_t len)
{
	/*
	 * Protocol defines subscribe as:
	 * SUB <queue name>\n
	 *
	 * Queues are persistent and so must be named.
	 */
	char name[QUEUE_NAME_MAX];
	if (sscanf(buf, "SUB %s", name) != 1)
		return -1;

	if (c->queue != NULL)
		return -1;

	struct queue *q = topic_find_create_queue(c->topic, name);
	if (q == NULL)
		return -1;

	/* queues need to add pending writes the client's event loop */
	if (queue_assign_write_event(q, c->ev_write) != 0)
		return -1;

	c->queue = q;

	return (ssize_t)len;
}

/*
 * cmsg_shutdown_handler -- handler of SHUTDOWN client message.
 *
 * Stops the broker.
 */
static ssize_t
cmsg_shutdown_handler(struct client *c, const char *buf, size_t len)
{
	topic_stop(c->topic);

	return -1;
}

/*
 * cmsg_bye_handler -- handler of BYE client message.
 *
 * Disconnects from the broker.
 */
static ssize_t
cmsg_bye_handler(struct client *c, const char *buf, size_t len)
{
	return -1;
}

/*
 * Client message handlers take tokenized buffers and return how many bytes were
 * consumed during processing. May return -1 to terminate the connection.
 */
typedef ssize_t (*msg_handler)(struct client *client,
	const char *buf, size_t len);

static msg_handler protocol_impl[MAX_CMSG] = {
	cmsg_publish_handler,
	cmsg_subscribe_handler,
	cmsg_shutdown_handler,
	cmsg_bye_handler
};

/*
 * on_cmsg -- calls the appropriate message handler
 */
static ssize_t
on_cmsg(struct client *c, const char *buf, size_t len)
{
	int i;
	for (i = 0; i < MAX_CMSG; ++i) {
		if (len >= strlen(cmsg_token[i]) &&
		    strncmp(cmsg_token[i], buf, strlen(cmsg_token[i])) == 0)
			break;
	}
	if (i == MAX_CMSG) {
		fprintf(stderr, "unknown or malformed client message\n");
		return -1;
	}

	return protocol_impl[i](c, buf, len);
}

/*
 * on_payload_read -- handle reads into pending message
 */
static int
on_payload_read(struct client *c, size_t read)
{
	TOID(struct message) m = message_get(c->pending);
	size_t nlen = message_length(m);
	if (c->buf_offset == nlen) /* if message completed */
		on_message_completion(c);

	return 0;
}

/*
 * on_frame_read -- handle regular reads into cmsg buffer
 */
static int
on_frame_read(struct client *c, size_t read)
{
	char *last; /* last byte of the message */
	char *buf = c->buf;
	size_t rlen = c->buf_offset; /* remaining data in the buffer */
	size_t data_len; /* length of the incoming message */

	/* all valid messages are terminated by MSG_END */
	while ((last = memchr(buf, MSG_END, rlen)) != NULL) {
		data_len = (size_t)(last - buf + 1);
		rlen -= data_len;

		ssize_t ret = on_cmsg(c, buf, data_len);
		if (ret < 0)
			return -1;
		else if (ret == 0)
			break;

		buf += ret;
	}

	/* if there are leftovers in the buffer, move them to pos 0 */
	if (rlen != 0)
		memcpy(c->buf, buf, rlen);

	c->buf_offset = rlen;

	return 0;
}

/*
 * on_read -- callback for read events
 *
 * Reads data into current buffer and calls the appropriate handler depending
 * on the application's state.
 */
static void
on_read(evutil_socket_t fd, short ev, void *arg)
{
	struct client *c = arg;
	ssize_t len = -1;

	size_t avail = c->buf_len - c->buf_offset;
	if (avail == 0) {
		printf("malformed data\n");
		goto err;
	}

	len = read(fd, c->buf + c->buf_offset, avail);
	if (len <= 0)
		goto err;

	c->buf_offset += (size_t)len;

	int ret = c->pending == NULL ?
		on_frame_read(c, (size_t)len) : on_payload_read(c, (size_t)len);
	if (ret != 0)
		goto err;

	return;

err:
	if (len == 0) {
		printf("client disconnect.\n");
	} else if (len < 0) {
		printf("client error: %s\n", strerror(errno));
	}

	client_delete(c);
}

/*
 * on_write -- callback for write events
 *
 * Write callback happens in once scenario: there's a pending message on our
 * current queue. This function takes the message currently at the head of the
 * queue and attempts to send it to the client's socket.
 */
static void
on_write(evutil_socket_t fd, short ev, void *arg)
{
	struct client *client = (struct client *)arg;
	struct queue *q = client->queue;

	TOID(struct message) msg = queue_peek(q); /* queue's head */

	/* can't rely on being able to write() the entire message */
	size_t len = message_length(msg) - client->write_offset;
	ssize_t ret = write(fd, message_data(msg) + client->write_offset, len);
	if (ret > 0) {
		if (ret < len) {
			client->write_offset += (size_t)ret;
		} else {
			/* if written the whole message, reset the offset ... */
			client->write_offset = 0;
			queue_pop(q); /* ... and pop msg from our queue */
		}
	}

	/* if the queue still isn't empty, schedule the write again */
	if (!queue_empty(q))
		event_add(client->ev_write, NULL);
}

/*
 * client_new -- creates a new client instance
 */
struct client *
client_new(PMEMobjpool *pop, struct topic *topic,
	int fd, struct event_base *evbase)
{
	struct client *c = malloc(sizeof(*c));
	if (c == NULL)
		return NULL;

	c->fd = fd;

	/* continuously (EV_PERSIST) wait for read (EV_READ) events */
	c->ev_read = event_new(evbase, fd, EV_READ | EV_PERSIST,
		on_read, c);
	/* wait for write (EV_WRITE) events only when necessary */
	c->ev_write = event_new(evbase, fd, EV_WRITE,
		on_write, c);

	c->pending = NULL;
	c->write_offset = 0;

	c->buf = c->cmsg;
	c->buf_offset = 0;
	c->buf_len = CLIENT_MSG_BUF;

	c->queue = NULL;
	c->topic = topic;
	c->pop = pop;

	event_add(c->ev_read, NULL);

	return c;
}

/*
 * client_delete -- deletes client instance and closes the socket
 */
void
client_delete(struct client *c)
{
	if (c->queue != NULL)
		queue_assign_write_event(c->queue, NULL);

	if (c->pending != NULL)
		message_pending_delete(c->pending);

	event_del(c->ev_read);
	event_del(c->ev_write);
	event_free(c->ev_read);
	event_free(c->ev_write);
	close(c->fd);

	free(c);
}
