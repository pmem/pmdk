// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmemd_obc_test_common.c -- common definitions for rpmemd_obc tests
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "os.h"

#include "rpmemd_obc_test_common.h"

#define CMD_BUFF_SIZE	4096
static const char *rpmem_cmd;

/*
 * set_rpmem_cmd -- set RPMEM_CMD variable
 */
void
set_rpmem_cmd(const char *fmt, ...)
{
	static char cmd_buff[CMD_BUFF_SIZE];

	if (!rpmem_cmd) {
		char *cmd = os_getenv(RPMEM_CMD_ENV);
		UT_ASSERTne(cmd, NULL);
		rpmem_cmd = STRDUP(cmd);
	}

	ssize_t ret;
	size_t cnt = 0;

	va_list ap;
	va_start(ap, fmt);
	ret = SNPRINTF(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt,
			"%s ", rpmem_cmd);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	ret = vsnprintf(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt, fmt, ap);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	va_end(ap);

	ret = os_setenv(RPMEM_CMD_ENV, cmd_buff, 1);
	UT_ASSERTeq(ret, 0);

	/*
	 * Rpmem has internal RPMEM_CMD variable copy and it is assumed
	 * RPMEMD_CMD will not change its value during execution. To refresh the
	 * internal copy it must be destroyed and a instance must be initialized
	 * manually.
	 */
	rpmem_util_cmds_fini();
	rpmem_util_cmds_init();
}

/*
 * req_cb_check_req -- validate request attributes
 */
static void
req_cb_check_req(const struct rpmem_req_attr *req)
{
	UT_ASSERTeq(req->nlanes, NLANES);
	UT_ASSERTeq(req->pool_size, POOL_SIZE);
	UT_ASSERTeq(req->provider, PROVIDER);
	UT_ASSERTeq(strcmp(req->pool_desc, POOL_DESC), 0);

}

/*
 * req_cb_check_pool_attr -- validate pool attributes
 */
static void
req_cb_check_pool_attr(const struct rpmem_pool_attr *pool_attr)
{
	struct rpmem_pool_attr attr = POOL_ATTR_INIT;
	UT_ASSERTeq(memcmp(&attr, pool_attr, sizeof(attr)), 0);
}

/*
 * req_cb_create -- callback for create request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_create(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr)
{
	UT_ASSERTne(arg, NULL);
	UT_ASSERTne(req, NULL);
	UT_ASSERTne(pool_attr, NULL);

	req_cb_check_req(req);
	req_cb_check_pool_attr(pool_attr);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_CREATE);

	int ret = args->ret;

	if (args->resp) {
		struct rpmem_resp_attr resp = {
			.port = PORT,
			.rkey = RKEY,
			.raddr = RADDR,
			.persist_method = PERSIST_METHOD,
			.nlanes = NLANES_RESP,
		};

		ret = rpmemd_obc_create_resp(obc,
				args->status, &resp);
	}

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_open -- callback for open request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_open(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req)
{
	UT_ASSERTne(arg, NULL);
	UT_ASSERTne(req, NULL);

	req_cb_check_req(req);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_OPEN);

	int ret = args->ret;

	if (args->resp) {
		struct rpmem_resp_attr resp = {
			.port = PORT,
			.rkey = RKEY,
			.raddr = RADDR,
			.persist_method = PERSIST_METHOD,
			.nlanes = NLANES_RESP,
		};

		struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

		ret = rpmemd_obc_open_resp(obc, args->status,
				&resp, &pool_attr);
	}

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_close -- callback for close request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_close(struct rpmemd_obc *obc, void *arg, int flags)
{
	UT_ASSERTne(arg, NULL);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_CLOSE);

	int ret = args->ret;

	if (args->resp)
		ret = rpmemd_obc_close_resp(obc, args->status);

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_set_attr -- callback for set attributes request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_set_attr(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_pool_attr *pool_attr)
{
	UT_ASSERTne(arg, NULL);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_SET_ATTR);

	int ret = args->ret;

	if (args->resp)
		ret = rpmemd_obc_set_attr_resp(obc, args->status);

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * REQ_CB -- request callbacks
 */
struct rpmemd_obc_requests REQ_CB = {
	.create = req_cb_create,
	.open = req_cb_open,
	.close = req_cb_close,
	.set_attr = req_cb_set_attr,
};

/*
 * clnt_wait_disconnect -- wait for disconnection
 */
void
clnt_wait_disconnect(struct rpmem_ssh *ssh)
{
	int ret;

	ret = rpmem_ssh_monitor(ssh, 0);
	UT_ASSERTne(ret, 1);
}

/*
 * clnt_connect -- create a ssh connection with specified target
 */
struct rpmem_ssh *
clnt_connect(char *target)
{

	struct rpmem_target_info *info;
	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	struct rpmem_ssh *ssh = rpmem_ssh_open(info);
	UT_ASSERTne(ssh, NULL);

	rpmem_target_free(info);

	return ssh;
}

/*
 * clnt_close -- close client
 */
void
clnt_close(struct rpmem_ssh *ssh)
{
	rpmem_ssh_close(ssh);
}

/*
 * clnt_send -- send data
 */
void
clnt_send(struct rpmem_ssh *ssh, const void *buff, size_t len)
{
	int ret;

	ret = rpmem_ssh_send(ssh, buff, len);
	UT_ASSERTeq(ret, 0);
}

/*
 * clnt_recv -- receive data
 */
void
clnt_recv(struct rpmem_ssh *ssh, void *buff, size_t len)
{
	int ret;

	ret = rpmem_ssh_recv(ssh, buff, len);
	UT_ASSERTeq(ret, 0);
}

/*
 * server_msg_args -- process a message according to specified arguments
 */
static void
server_msg_args(struct rpmemd_obc *rpdc, enum conn_wait_close conn,
	struct req_cb_arg *args)
{
	int ret;
	unsigned long long types = args->types;
	args->types = 0;

	ret = rpmemd_obc_process(rpdc, &REQ_CB, args);
	UT_ASSERTeq(ret, args->ret);
	UT_ASSERTeq(args->types, types);

	if (conn == CONN_WAIT_CLOSE) {
		ret = rpmemd_obc_process(rpdc, &REQ_CB, args);
		UT_ASSERTeq(ret, 1);
	}

	rpmemd_obc_fini(rpdc);
}

/*
 * server_msg_resp -- process a message of specified type, response to client
 * with specific status value and return status of sending response function
 */
int
server_msg_resp(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: %s msg_type status", tc->name);

	unsigned type = ATOU(argv[0]);
	int status = atoi(argv[1]);

	int ret;
	struct rpmemd_obc *rpdc;

	rpdc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_status(rpdc, 0);
	UT_ASSERTeq(ret, 0);

	struct req_cb_arg args = {
		.ret = 0,
		.force_ret = 0,
		.resp = 1,
		.types = (1U << type),
		.status = status,
	};

	server_msg_args(rpdc, CONN_WAIT_CLOSE, &args);

	return 2;
}

/*
 * server_msg_noresp -- process a message of specified type, do not response to
 * client and return specific value from process callback
 */
int
server_msg_noresp(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s msg_type", tc->name);

	int type = atoi(argv[0]);
	int ret;
	struct rpmemd_obc *rpdc;

	rpdc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_status(rpdc, 0);
	UT_ASSERTeq(ret, 0);

	struct req_cb_arg args = {
		.ret = -1,
		.force_ret = 1,
		.resp = 0,
		.types = (1U << type),
		.status = 0,
	};

	server_msg_args(rpdc, CONN_CLOSE, &args);

	return 1;
}

/*
 * server_bad_msg -- process a message and expect
 * error returned from rpmemd_obc_process function
 */
int
server_bad_msg(const struct test_case *tc, int argc, char *argv[])
{
	int ret;
	struct rpmemd_obc *rpdc;

	rpdc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_status(rpdc, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_obc_process(rpdc, &REQ_CB, NULL);
	UT_ASSERTne(ret, 0);

	rpmemd_obc_fini(rpdc);

	return 0;
}
