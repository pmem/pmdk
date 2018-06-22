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
 * protocol.h - human readable message broker protocol
 */

/*
 * All client messages must start with a valid message token and be terminated
 * by a newline character ('\n'). The message parser is case-sensitive.
 *
 * Server responds with newline terminated string literals.
 * If invalid message token is received, the connection is terminated.
 */
enum cmsg {
	/*
	 * PUBLISH client message
	 * Syntax: PUB <data length>\n<data ...>
	 *
	 * Data length is limited by PMEMOBJ_MAX_ALLOC_SIZE.
	 *
	 * Operation publishes a new message to the default topic. The message
	 * will be persistently stored and eventually sent out to all
	 * subscribing connections.
	 */
	CMSG_PUBLISH,

	/*
	 * SUBSCRIBE client message
	 * Syntax: SUB <queue name>\n
	 *
	 * Queue name must be at most 8 bytes.
	 *
	 * Operation creates or finds and existing queue with the given name and
	 * attaches it to the client's connection. Can only be called once
	 * during a single connection.
	 * If there are pending messages on the queue, they are all sent
	 * to the client.
	 */
	CMSG_SUBSCRIBE,

	/*
	 * SHUTDOWN client message
	 * Syntax: SHUTDOWN\n
	 *
	 * Operation terminates the client connection and gracefully shutdowns
	 * the server.
	 */
	CMSG_SHUTDOWN,

	/*
	 * BYE client message
	 * Syntax: BYE\n
	 *
	 * Operation terminates the client connection.
	 * No return value.
	 */
	CMSG_BYE,

	MAX_CMSG,
};

static const char *cmsg_token[MAX_CMSG] = {
	[CMSG_PUBLISH] = "PUB",
	[CMSG_SUBSCRIBE] = "SUB",
	[CMSG_SHUTDOWN] = "SHUTDOWN",
	[CMSG_BYE] = "BYE",
};

#define MSG_END '\n'
