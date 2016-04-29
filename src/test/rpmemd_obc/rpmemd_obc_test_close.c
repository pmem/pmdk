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
 * rpmemd_obc_test_close.c -- test cases for close request message
 */

#include "rpmemd_obc_test_common.h"

/*
 * client_msg_close_noresp -- send close request message and don't expect a
 * response
 */
static void
client_msg_close_noresp(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	struct rpmem_msg_close msg = CLOSE_MSG;
	rpmem_hton_msg_close(&msg);

	int fd = clnt_connect_wait(target);

	clnt_send(fd, &msg, sizeof(msg));
	clnt_wait_disconnect(fd);
	clnt_close(fd);

	FREE(target);
}

/*
 * client_msg_close_resp -- send close request message and expect a response
 * with specified status. If status is 0, validate close request response
 * message
 */
static void
client_msg_close_resp(const char *ctarget, int status)
{
	char *target = STRDUP(ctarget);
	struct rpmem_msg_close msg = CLOSE_MSG;
	rpmem_hton_msg_close(&msg);
	struct rpmem_msg_close_resp resp;

	int fd = clnt_connect_wait(target);

	clnt_send(fd, &msg, sizeof(msg));
	clnt_recv(fd, &resp, sizeof(resp));
	rpmem_ntoh_msg_close_resp(&resp);

	if (status)
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);

	clnt_close(fd);

	FREE(target);
}

/*
 * client_close -- test case for close request message - client side
 */
void
client_close(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	client_msg_close_noresp(target);

	client_msg_close_resp(target, 0);
	client_msg_close_resp(target, 1);
}

/*
 * server_close -- test case for close request message - server side
 */
void
server_close(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	int ret;

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	server_msg_noresp(rpdc, RPMEM_MSG_TYPE_CLOSE);

	server_msg_resp(rpdc, RPMEM_MSG_TYPE_CLOSE, 0);
	server_msg_resp(rpdc, RPMEM_MSG_TYPE_CLOSE, 1);

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}
