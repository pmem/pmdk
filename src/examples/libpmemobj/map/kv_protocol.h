/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * kv_protocol.h -- kv store text protocol
 */

#ifndef KV_PROTOCOL_H
#define KV_PROTOCOL_H

#include <stdint.h>

#define MAX_KEY_LEN 255

/*
 * All client messages must start with a valid message token and be terminated
 * by a newline character ('\n'). The message parser is case-sensitive.
 *
 * Server responds with newline terminated string literals.
 * If invalid message token is received RESP_MSG_UNKNOWN is sent.
 */

enum kv_cmsg {
	/*
	 * INSERT client message
	 * Syntax: INSERT [key] [value]\n
	 *
	 * The key is limited to 255 characters, the size of a value is limited
	 * by the pmemobj maximum allocation size (~16 gigabytes).
	 *
	 * Operation adds a new key value pair to the map.
	 * Returns RESP_MSG_SUCCESS if successful or RESP_MSG_FAIL otherwise.
	 */
	CMSG_INSERT,

	/*
	 * REMOVE client message
	 * Syntax: REMOVE [key]\n
	 *
	 * Operation removes a key value pair from the map.
	 * Returns RESP_MSG_SUCCESS if successful or RESP_MSG_FAIL otherwise.
	 */
	CMSG_REMOVE,

	/*
	 * GET client message
	 * Syntax: GET [key]\n
	 *
	 * Operation retrieves a key value pair from the map.
	 * Returns the value if found or RESP_MSG_NULL otherwise.
	 */
	CMSG_GET,

	/*
	 * BYE client message
	 * Syntax: BYE\n
	 *
	 * Operation terminates the client connection.
	 * No return value.
	 */
	CMSG_BYE,

	/*
	 * KILL client message
	 * Syntax: KILL\n
	 *
	 * Operation terminates the client connection and gracefully shutdowns
	 * the server.
	 * No return value.
	 */
	CMSG_KILL,

	MAX_CMSG
};

enum resp_messages {
	RESP_MSG_SUCCESS,
	RESP_MSG_FAIL,
	RESP_MSG_NULL,
	RESP_MSG_UNKNOWN,

	MAX_RESP_MSG
};

static const char *resp_msg[MAX_RESP_MSG] = {
	[RESP_MSG_SUCCESS] = "SUCCESS\n",
	[RESP_MSG_FAIL] = "FAIL\n",
	[RESP_MSG_NULL] = "NULL\n",
	[RESP_MSG_UNKNOWN] = "UNKNOWN\n"
};

static const char *kv_cmsg_token[MAX_CMSG] = {
	[CMSG_INSERT] = "INSERT",
	[CMSG_REMOVE] = "REMOVE",
	[CMSG_GET] = "GET",
	[CMSG_BYE] = "BYE",
	[CMSG_KILL] = "KILL"
};

#endif /* KV_PROTOCOL_H */
