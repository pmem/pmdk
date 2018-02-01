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
 * broker.c - simple message broker implemented using libpmemobj to support
 *	persistent queues and messages.
 */

#include <event2/event.h>
#include <event2/thread.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <libpmemobj.h>
#include "client.h"

#define CONN_BACKLOG 16
#define NWORKERS 8
#define NWORKERS_MASK (NWORKERS - 1)

static PMEMobjpool *pop;

/*
 * event loop worker that is responsible for dispatching read/write events
 * to client connections. There can be many of these workers running
 * concurrently to increase throughput of broker.
 */
struct worker {
	pthread_t th;
	struct event_base *evbase;
	int running;
} workers[NWORKERS];

/*
 * Topic to which messages are sent. For simplicity sake, there's only one topic
 * in the broker.
 */
struct topic *topic;


/*
 * worker_next -- selects the next worker for the client connection in a
 *	round robin fashion
 */
static struct worker *
worker_next()
{
	static uint64_t worker_id = 0;

	return &workers[__sync_fetch_and_add(&worker_id, 1) & NWORKERS_MASK];
}

/*
 * worker_func -- worker event loop, handles read and write events
 */
static void *
worker_func(void *arg)
{
	struct worker *w = arg;
	w->evbase = event_base_new();
	w->running = 1;

	struct timeval heartbeat;
	heartbeat.tv_sec = 5;
	heartbeat.tv_usec = 0;

	while (__atomic_load_n(&w->running, __ATOMIC_ACQUIRE)) {
		event_base_loopexit(w->evbase, &heartbeat);

		if (event_base_dispatch(w->evbase) < 0) {
			printf("event_base_dispatch %s\n", strerror(errno));
			break;
		}
	}

	event_base_free(w->evbase);

	return NULL;
}

/*
 * workers_run -- launches the worker threads
 */
static void
workers_run()
{
	evthread_use_pthreads();
	for (int i = 0; i < NWORKERS; ++i) {
		struct worker *w = &workers[i];
		if (pthread_create(&w->th, NULL, worker_func, w) < 0)
			assert(0);
	}
}

/*
 * workers_stop -- terminates the event loop and waits for the threads to exit
 */
static void
workers_stop()
{
	for (int i = 0; i < NWORKERS; ++i) {
		struct worker *w = &workers[i];
		w = &workers[i];
		if (__sync_bool_compare_and_swap(&w->running, 1, 0)) {
			event_base_loopbreak(w->evbase);
			pthread_join(w->th, NULL);
		}
	}
}

/*
 * on_accept -- callback for incoming client connection, creates a new client
 *	instance which attaches itself to one of the workers
 */
static void
on_accept(evutil_socket_t fd, short ev, void *arg)
{
	int cfd;
	struct sockaddr_in addr;

	socklen_t len = sizeof(addr);
	if ((cfd = accept(fd, (struct sockaddr *)&addr, &len)) < 0)
		return;

	if (evutil_make_socket_nonblocking(cfd) < 0)
		goto err;

	if (client_new(pop, topic, cfd, worker_next()->evbase) == NULL)
		goto err;

	return;
err:
	close(cfd);
}

/*
 * server_run -- setups the server socket and the event loop for the accept
 *	events (reads)
 *
 * This function is blocking, terminates only if the event loop is interrupted.
 */
static int
server_run(int port, struct event_base *evbase)
{
	evutil_socket_t fd;
	struct sockaddr_in addr;
	struct event *ev_accept;

	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons((uint16_t)port);
	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		goto err;

	if (listen(fd, CONN_BACKLOG) < 0)
		goto err;

	int reuse = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	if (evutil_make_socket_nonblocking(fd) < 0)
		goto err;

	ev_accept = event_new(evbase, fd, EV_READ | EV_PERSIST,
		on_accept, NULL);

	event_add(ev_accept, NULL);

	event_base_dispatch(evbase);

	event_free(ev_accept);
	event_base_free(evbase);

	close(fd);

	return 0;

err:
	close(fd);
	return -1;
}

/*
 * main -- broker start
 */
int
main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s file-name port\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];
	int port = atoi(argv[2]);

	/* 1. open the pool with queues and messages */
	pop = pmemobj_open(path, POBJ_LAYOUT_NAME(broker));
	if (pop == NULL) {
		fprintf(stderr, "failed to open pool: %s\n",
				pmemobj_errormsg());
		return 1;
	}

	/* 2. recover all existing queues and attached messages */
	queue_recover_all(pop);

	/* 3. run event loop worker threads */
	workers_run();

	/*
	 * The server event loop on which the broker listens for connections.
	 * Can be used to terminate the application.
	 */
	struct event_base *server_evbase = event_base_new();

	/* 4. create transient topic instance */
	topic = topic_new(pop, "default", server_evbase);

	/* 5. start the server */
	if (server_run(port, server_evbase) != 0) {
		fprintf(stderr, "failed to run the server on the given port\n");
		topic_stop(topic);
	}

	/* cleanup */
	workers_stop();
	topic_delete(topic);
	event_base_free(server_evbase);
	pmemobj_close(pop);

	return 0;
}
