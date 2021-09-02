// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * kv_server.c -- persistent tcp key-value store server
 */

#include <uv.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libpmemobj.h"

#include "map.h"
#include "map_ctree.h"
#include "map_btree.h"
#include "map_rtree.h"
#include "map_rbtree.h"
#include "map_hashmap_atomic.h"
#include "map_hashmap_tx.h"
#include "map_hashmap_rp.h"
#include "map_skiplist.h"

#include "kv_protocol.h"

#define COUNT_OF(x) (sizeof(x) / sizeof(0[x]))
#define COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))

POBJ_LAYOUT_BEGIN(kv_server);
POBJ_LAYOUT_ROOT(kv_server, struct root);
POBJ_LAYOUT_TOID(kv_server, struct map_value);
POBJ_LAYOUT_TOID(kv_server, uint64_t);
POBJ_LAYOUT_END(kv_server);

struct map_value {
	uint64_t len;
	char buf[];
};

struct root {
	TOID(struct map) map;
};

static struct map_ctx *mapc;
static PMEMobjpool *pop;
static TOID(struct map) map;

static uv_tcp_t server;
static uv_loop_t *loop;

typedef int (*msg_handler)(uv_stream_t *client, const char *msg, size_t len);

struct write_req {
	uv_write_t req;
	uv_buf_t buf;
};

struct client_data {
	char *buf; /* current message, always NULL terminated */
	size_t buf_len; /* sizeof(buf) */
	size_t len; /* actual length of the message (while parsing) */
};

/*
 * djb2_hash -- string hashing function by Dan Bernstein
 */
static uint32_t
djb2_hash(const char *str)
{
	uint32_t hash = 5381;
	int c;

	while ((c = *str++))
		hash = ((hash << 5) + hash) + c;

	return hash;
}

/*
 * write_done_cb -- callback after message write completes
 */
static void
write_done_cb(uv_write_t *req, int status)
{
	struct write_req *wr = (struct write_req *)req;
	free(wr);

	if (status == -1) {
		printf("response failed");
	}
}

/*
 * client_close_cb -- callback after client tcp connection closes
 */
static void
client_close_cb(uv_handle_t *handle)
{
	struct client_data *d = handle->data;
	free(d->buf);
	free(handle->data);
	free(handle);
}

/*
 * response_write -- response writing helper
 */
static void
response_write(uv_stream_t *client, char *resp, size_t len)
{
	struct write_req *wr = malloc(sizeof(struct write_req));
	assert(wr != NULL);

	wr->buf = uv_buf_init(resp, len);
	uv_write(&wr->req, client, &wr->buf, 1, write_done_cb);
}

/*
 * response_msg -- predefined message writing helper
 */
static void
response_msg(uv_stream_t *client, enum resp_messages msg)
{
	response_write(client, (char *)resp_msg[msg], strlen(resp_msg[msg]));
}

/*
 * cmsg_insert_handler -- handler of INSERT client message
 */
static int
cmsg_insert_handler(uv_stream_t *client, const char *msg, size_t len)
{
	int result = 0;
	TX_BEGIN(pop) {
		/*
		 * For simplicity sake the length of the value buffer is just
		 * a length of the message.
		 */
		TOID(struct map_value) val = TX_ZALLOC(struct map_value,
			sizeof(struct map_value) + len);

		char key[MAX_KEY_LEN];
		int ret = sscanf(msg, "INSERT %254s %s\n", key, D_RW(val)->buf);
		assert(ret == 2);

		D_RW(val)->len = len;

		/* properly terminate the value */
		D_RW(val)->buf[strlen(D_RO(val)->buf)] = '\n';

		map_insert(mapc, map, djb2_hash(key), val.oid);
	} TX_ONABORT {
		result = 1;
	} TX_END

	response_msg(client, result);

	return 0;
}

/*
 * cmsg_remove_handler -- handler of REMOVE client message
 */
static int
cmsg_remove_handler(uv_stream_t *client, const char *msg, size_t len)
{
	char key[MAX_KEY_LEN] = {0};

	/* check if the constant used in sscanf() below has the correct value */
	COMPILE_ERROR_ON(MAX_KEY_LEN - 1 != 254);
	int ret = sscanf(msg, "REMOVE %254s\n", key);
	assert(ret == 1);

	int result = map_remove_free(mapc, map, djb2_hash(key));

	response_msg(client, result);

	return 0;
}

/*
 * cmsg_get_handler -- handler of GET client message
 */
static int
cmsg_get_handler(uv_stream_t *client, const char *msg, size_t len)
{
	char key[MAX_KEY_LEN];

	/* check if the constant used in sscanf() below has the correct value */
	COMPILE_ERROR_ON(MAX_KEY_LEN - 1 != 254);
	int ret = sscanf(msg, "GET %254s\n", key);
	assert(ret == 1);

	TOID(struct map_value) value;
	TOID_ASSIGN(value, map_get(mapc, map, djb2_hash(key)));

	if (TOID_IS_NULL(value)) {
		response_msg(client, RESP_MSG_NULL);
	} else {
		response_write(client, D_RW(value)->buf, D_RO(value)->len);
	}

	return 0;
}

/*
 * cmsg_bye_handler -- handler of BYE client message
 */
static int
cmsg_bye_handler(uv_stream_t *client, const char *msg, size_t len)
{
	uv_close((uv_handle_t *)client, client_close_cb);

	return 0;
}

/*
 * cmsg_bye_handler -- handler of KILL client message
 */
static int
cmsg_kill_handler(uv_stream_t *client, const char *msg, size_t len)
{
	uv_close((uv_handle_t *)client, client_close_cb);
	uv_close((uv_handle_t *)&server, NULL);

	return 0;
}

/* kv protocol implementation */
static msg_handler protocol_impl[MAX_CMSG] = {
	cmsg_insert_handler,
	cmsg_remove_handler,
	cmsg_get_handler,
	cmsg_bye_handler,
	cmsg_kill_handler
};

/*
 * cmsg_handle -- handles current client message
 */
static int
cmsg_handle(uv_stream_t *client, struct client_data *data)
{
	int ret = 0;
	int i;
	for (i = 0; i < MAX_CMSG; ++i)
		if (strncmp(kv_cmsg_token[i], data->buf,
			strlen(kv_cmsg_token[i])) == 0)
			break;

	if (i == MAX_CMSG) {
		response_msg(client, RESP_MSG_UNKNOWN);
	} else {
		ret = protocol_impl[i](client, data->buf, data->len);
	}

	data->len = 0; /* reset the message length */

	return ret;
}

/*
 * cmsg_handle_stream -- handle incoming tcp stream from clients
 */
static int
cmsg_handle_stream(uv_stream_t *client, struct client_data *data,
	const char *buf, ssize_t nread)
{
	char *last;
	int ret;
	size_t len;

	/*
	 * A single read operation can contain zero or more operations, so this
	 * has to be handled appropriately. Client messages are terminated by
	 * newline character.
	 */
	while ((last = memchr(buf, '\n', nread)) != NULL) {
		len = last - buf + 1;
		nread -= len;
		assert(data->len + len <= data->buf_len);
		memcpy(data->buf + data->len, buf, len);
		data->len += len;

		if ((ret = cmsg_handle(client, data)) != 0)
			return ret;

		buf = last + 1;
	}

	if (nread != 0) {
		memcpy(data->buf + data->len, buf, nread);
		data->len += nread;
	}

	return 0;
}

static uv_buf_t msg_buf = {0};

/*
 * get_read_buf_cb -- returns buffer for incoming client message
 */
static void
get_read_buf_cb(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
	buf->base = msg_buf.base;
	buf->len = msg_buf.len;
}

/*
 * read_cb -- async tcp read from clients
 */
static void
read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf)
{
	if (nread <= 0) {
		printf("client connection closed\n");
		uv_close((uv_handle_t *)client, client_close_cb);

		return;
	}

	struct client_data *d = client->data;

	if (d->buf_len < (d->len + nread + 1)) {
		char *cbuf = realloc(d->buf, d->buf_len + nread + 1);
		assert(cbuf != NULL);

		/* zero only the new memory */
		memset(cbuf + d->buf_len, 0, nread + 1);

		d->buf_len += nread + 1;
		d->buf = cbuf;
	}

	if (cmsg_handle_stream(client, client->data, buf->base, nread)) {
		printf("client disconnect\n");
		uv_close((uv_handle_t *)client, client_close_cb);
	}
}

/*
 * connection_cb -- async incoming client request
 */
static void
connection_cb(uv_stream_t *_server, int status)
{
	if (status != 0) {
		printf("client connect error\n");
		return;
	}
	printf("new client\n");

	uv_tcp_t *client = malloc(sizeof(uv_tcp_t));
	assert(client != NULL);
	client->data = calloc(1, sizeof(struct client_data));
	assert(client->data != NULL);

	uv_tcp_init(loop, client);

	if (uv_accept(_server, (uv_stream_t *)client) == 0) {
		uv_read_start((uv_stream_t *)client, get_read_buf_cb, read_cb);
	} else {
		uv_close((uv_handle_t *)client, client_close_cb);
	}
}

static const struct {
	struct map_ops *ops;
	const char *name;
} maps[] = {
	{MAP_HASHMAP_TX, "hashmap_tx"},
	{MAP_HASHMAP_ATOMIC, "hashmap_atomic"},
	{MAP_HASHMAP_RP, "hashmap_rp"},
	{MAP_CTREE, "ctree"},
	{MAP_BTREE, "btree"},
	{MAP_RTREE, "rtree"},
	{MAP_RBTREE, "rbtree"},
	{MAP_SKIPLIST, "skiplist"}
};

/*
 * get_map_ops_by_string -- parse the type string and return the associated ops
 */
static const struct map_ops *
get_map_ops_by_string(const char *type)
{
	for (int i = 0; i < COUNT_OF(maps); ++i)
		if (strcmp(maps[i].name, type) == 0)
			return maps[i].ops;

	return NULL;
}

#define KV_SIZE	(PMEMOBJ_MIN_POOL)

#define MAX_READ_LEN (64 * 1024) /* 64 kilobytes */

int
main(int argc, char *argv[])
{
	if (argc < 4) {
		printf("usage: %s hashmap_tx|hashmap_atomic|hashmap_rp|"
				"ctree|btree|rtree|rbtree|skiplist file-name port\n",
				argv[0]);
		return 1;
	}

	const char *path = argv[2];
	const char *type = argv[1];
	int port = atoi(argv[3]);

	/* use only a single buffer for all incoming data */
	void *read_buf = malloc(MAX_READ_LEN);
	assert(read_buf != NULL);

	msg_buf = uv_buf_init(read_buf, MAX_READ_LEN);

	if (access(path, F_OK) != 0) {
		pop = pmemobj_create(path, POBJ_LAYOUT_NAME(kv_server),
				KV_SIZE, 0666);
		if (pop == NULL) {
			fprintf(stderr, "failed to create pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}
	} else {
		pop = pmemobj_open(path, POBJ_LAYOUT_NAME(kv_server));
		if (pop == NULL) {
			fprintf(stderr, "failed to open pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}
	}

	/* map context initialization */
	mapc = map_ctx_init(get_map_ops_by_string(type), pop);
	if (!mapc) {
		pmemobj_close(pop);
		fprintf(stderr, "map_ctx_init failed (wrong type?)\n");
		return 1;
	}

	/* initialize the actual map */
	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	if (TOID_IS_NULL(D_RO(root)->map)) {
		/* create new if it doesn't exist (a fresh pool) */
		map_create(mapc, &D_RW(root)->map, NULL);
	}
	map = D_RO(root)->map;

	loop = uv_default_loop();

	/* tcp server initialization */
	uv_tcp_init(loop, &server);

	struct sockaddr_in bind_addr;
	uv_ip4_addr("0.0.0.0", port, &bind_addr);
	int ret = uv_tcp_bind(&server, (const struct sockaddr *)&bind_addr, 0);
	assert(ret == 0);

	ret = uv_listen((uv_stream_t *)&server, SOMAXCONN, connection_cb);
	assert(ret == 0);

	ret = uv_run(loop, UV_RUN_DEFAULT);
	assert(ret == 0);

	/* no more events in the loop, release resources and quit */
	uv_loop_delete(loop);
	map_ctx_free(mapc);
	pmemobj_close(pop);

	free(read_buf);

	return 0;
}
