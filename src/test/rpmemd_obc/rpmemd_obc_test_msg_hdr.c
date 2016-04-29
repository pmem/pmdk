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
 * rpmemd_obc_test_msg_hdr.c -- test cases for message header
 */

#include "rpmemd_obc_test_common.h"

/*
 * Number of cases for checking message header. Must be kept in sync with
 * client_bad_msg_hdr function.
 */
#define BAD_MSG_HDR_COUNT	7

/*
 * client_bad_msg_hdr -- test case for checking message header
 */
void
client_bad_msg_hdr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < BAD_MSG_HDR_COUNT; i++) {
		int fd = clnt_connect_wait(target);

		struct rpmem_msg_hdr msg = MSG_HDR;

		switch (i) {
		case 0:
			msg.size -= 1;
			break;
		case 1:
			msg.size = 0;
			break;
		case 2:
			msg.type = MAX_RPMEM_MSG_TYPE;
			break;
		case 3:
			msg.type = RPMEM_MSG_TYPE_OPEN_RESP;
			break;
		case 4:
			msg.type = RPMEM_MSG_TYPE_CREATE_RESP;
			break;
		case 5:
			msg.type = RPMEM_MSG_TYPE_CLOSE_RESP;
			break;
		case 6:
			msg.type = RPMEM_MSG_TYPE_REMOVE_RESP;
			break;
		default:
			UT_ASSERT(0);
		}

		rpmem_hton_msg_hdr(&msg);

		clnt_send(fd, &msg, sizeof(msg));
		clnt_wait_disconnect(fd);
		clnt_close(fd);
	}
}

/*
 * server_bad_msg_hdr -- test case for checking message header
 */
void
server_bad_msg_hdr(const struct test_case *tc, int argc, char *argv[])
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

	server_bad_msg(rpdc, BAD_MSG_HDR_COUNT);

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}
