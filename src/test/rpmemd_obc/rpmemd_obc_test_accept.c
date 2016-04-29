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
 * rpmemd_obc_test_misc.c -- test cases for rpmemd_obc_accept
 */

#include "rpmemd_obc_test_common.h"

#define ACCEPT_COUNT	10

/*
 * client_accept_seq -- establish multiple connections with server sequentially
 * and disconnect immediately
 */
void
client_accept_seq(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	int fd[ACCEPT_COUNT];
	char *target = argv[0];

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		fd[i] = clnt_connect(target);
		UT_ASSERTne(fd[i], -1);
		clnt_close(fd[i]);
	}
}

/*
 * server_accept_seq -- accept multiple connections sequentially and wait for
 * disconnect
 */
void
server_accept_seq(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	struct rpmemd_obc_client *client[ACCEPT_COUNT];
	int ret;

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		client[i] = rpmemd_obc_accept(rpdc);
		UT_ASSERTne(client[i], NULL);

		ret = rpmemd_obc_client_process(client[i], &REQ_CB, NULL);
		UT_ASSERTeq(ret, 1);

		ret = rpmemd_obc_client_close(client[i]);
		UT_ASSERTeq(ret, 0);

		rpmemd_obc_client_fini(client[i]);
	}

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}

/*
 * server_accept_seq_fork -- accept multiple connections sequentially and wait
 * for disconnect, each connection in separate process
 */
void
server_accept_seq_fork(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	struct rpmemd_obc_client *client;
	int ret;

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		client = rpmemd_obc_accept(rpdc);
		UT_ASSERTne(client, NULL);

		pid_t pid = fork();
		UT_ASSERTne(pid, -1);

		if (!pid) {
			ret = rpmemd_obc_close(rpdc);
			UT_ASSERTeq(ret, 0);

			rpmemd_obc_fini(rpdc);

			ret = rpmemd_obc_client_process(client, &REQ_CB, NULL);
			UT_ASSERTeq(ret, 1);

			ret = rpmemd_obc_client_close(client);
			UT_ASSERTeq(ret, 0);

			rpmemd_obc_client_fini(client);

			exit(EXIT_SUCCESS);
		}

		ret = rpmemd_obc_client_close(client);
		UT_ASSERTeq(ret, 0);

		rpmemd_obc_client_fini(client);

		pid_t wpid = waitpid(pid, &ret, 0);
		UT_ASSERTeq(wpid, pid);
		UT_ASSERTeq(ret, 0);
	}

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}

/*
 * client_accept_sim -- establish multiple connections with server
 * simultaneously and disconnect immediately
 */
void
client_accept_sim(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	int fd[ACCEPT_COUNT];
	char *target = argv[0];

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		fd[i] = clnt_connect(target);
		UT_ASSERTne(fd[i], -1);
	}

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		clnt_close(fd[i]);
	}
}

/*
 * server_accept_sim -- accept multiple connections simultaneously and
 * wait for disconnect
 */
void
server_accept_sim(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	struct rpmemd_obc_client *client[ACCEPT_COUNT];
	int ret;

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		client[i] = rpmemd_obc_accept(rpdc);
		UT_ASSERTne(client[i], NULL);
	}

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		ret = rpmemd_obc_client_process(client[i], &REQ_CB, NULL);
		UT_ASSERTeq(ret, 1);

		ret = rpmemd_obc_client_close(client[i]);
		UT_ASSERTeq(ret, 0);

		rpmemd_obc_client_fini(client[i]);
	}

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}

/*
 * server_accept_sim_fork -- accept multiple connections simultaneously and
 * wait for disconnect, each connection in separate process
 */
void
server_accept_sim_fork(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	struct rpmemd_obc_client *client;
	int ret;
	pid_t child[ACCEPT_COUNT];

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		client = rpmemd_obc_accept(rpdc);
		UT_ASSERTne(client, NULL);

		pid_t pid = fork();
		UT_ASSERTne(pid, -1);

		if (!pid) {
			ret = rpmemd_obc_close(rpdc);
			UT_ASSERTeq(ret, 0);

			rpmemd_obc_fini(rpdc);

			ret = rpmemd_obc_client_process(client, &REQ_CB, NULL);
			UT_ASSERTeq(ret, 1);

			ret = rpmemd_obc_client_close(client);
			UT_ASSERTeq(ret, 0);

			rpmemd_obc_client_fini(client);

			exit(EXIT_SUCCESS);
		}

		ret = rpmemd_obc_client_close(client);
		UT_ASSERTeq(ret, 0);

		rpmemd_obc_client_fini(client);

		child[i] = pid;
	}

	for (int i = 0; i < ACCEPT_COUNT; i++) {
		pid_t wpid = waitpid(child[i], &ret, 0);
		UT_ASSERTeq(wpid, child[i]);
		UT_ASSERTeq(ret, 0);
	}

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}
