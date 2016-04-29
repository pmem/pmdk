/*
 * Copyright 2015-2016, Intel Corporation
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
