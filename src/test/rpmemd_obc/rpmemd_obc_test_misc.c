/*
 * Copyright 2016, Intel Corporation
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
 * rpmemd_obc_test_misc.c -- miscellaneous test cases for rpmemd_obc module
 */

#include "rpmemd_obc_test_common.h"

/*
 * client_send_disconnect -- connect, send specified number of bytes and
 * disconnect
 */
static void
client_send_disconnect(char *target, void *msg, size_t size)
{
	struct rpmem_ssh *ssh = clnt_connect(target);

	if (size)
		clnt_send(ssh, msg, size);

	clnt_close(ssh);
}

/*
 * client_econnreset -- test case for closing connection when operation on
 * server is in progress - client side
 */
int
client_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	size_t msg_size = sizeof(CREATE_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_create *msg = MALLOC(msg_size);

	*msg = CREATE_MSG;
	msg->hdr.size = msg_size;
	memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);
	rpmem_hton_msg_create(msg);

	set_rpmem_cmd("server_econnreset");

	{
		/*
		 * Connect and disconnect immediately.
		 */
		client_send_disconnect(target, msg, 0);
	}

	{
		/*
		 * Connect, send half of a message header and close the
		 * connection.
		 */
		client_send_disconnect(target, msg,
				sizeof(struct rpmem_msg_hdr) / 2);
	}

	{
		/*
		 * Connect, send only a message header and close the
		 * connection.
		 */
		client_send_disconnect(target, msg,
				sizeof(struct rpmem_msg_hdr));
	}

	{
		/*
		 * Connect, send half of a message and close the connection.
		 */
		client_send_disconnect(target, msg, msg_size / 2);
	}

	FREE(msg);

	return 1;
}

/*
 * server_econnreset -- test case for closing connection when operation on
 * server is in progress - server side
 */
int
server_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	struct rpmemd_obc *rpdc;
	int ret;

	rpdc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_status(rpdc, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_obc_process(rpdc, &REQ_CB, NULL);
	UT_ASSERTne(ret, 0);

	rpmemd_obc_fini(rpdc);

	return 0;
}
