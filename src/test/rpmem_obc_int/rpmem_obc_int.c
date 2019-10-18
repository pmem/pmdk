/*
 * Copyright 2016-2019, Intel Corporation
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
 * rpmem_obc_int.c -- integration test for rpmem_obc and rpmemd_obc modules
 */

#include "unittest.h"
#include "pmemcommon.h"

#include "librpmem.h"
#include "rpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_obc.h"
#include "rpmemd_obc.h"
#include "rpmemd_log.h"
#include "os.h"

#define POOL_SIZE	1024
#define NLANES		32
#define NLANES_RESP	16
#define PROVIDER	RPMEM_PROV_LIBFABRIC_SOCKETS
#define POOL_DESC	"pool_desc"
#define RKEY		0xabababababababab
#define RADDR		0x0101010101010101
#define PORT		1234
#define PERSIST_METHOD	RPMEM_PM_GPSPM
#define RESP_ATTR_INIT {\
	.port = PORT,\
	.rkey = RKEY,\
	.raddr = RADDR,\
	.persist_method = PERSIST_METHOD,\
	.nlanes = NLANES_RESP,\
}
#define REQ_ATTR_INIT {\
	.pool_size = POOL_SIZE,\
	.nlanes = NLANES,\
	.provider = PROVIDER,\
	.pool_desc = POOL_DESC,\
}
#define POOL_ATTR_INIT {\
	.signature		= "<RPMEM>",\
	.major			= 1,\
	.compat_features	= 2,\
	.incompat_features	= 3,\
	.ro_compat_features	= 4,\
	.poolset_uuid		= "POOLSET_UUID0123",\
	.uuid			= "UUID0123456789AB",\
	.next_uuid		= "NEXT_UUID0123456",\
	.prev_uuid		= "PREV_UUID0123456",\
	.user_flags		= "USER_FLAGS012345",\
}
#define POOL_ATTR_ALT {\
	.signature		= "<ALT>",\
	.major			= 5,\
	.compat_features	= 6,\
	.incompat_features	= 7,\
	.ro_compat_features	= 8,\
	.poolset_uuid		= "UUID_POOLSET_ALT",\
	.uuid			= "ALT_UUIDCDEFFEDC",\
	.next_uuid		= "456UUID_NEXT_ALT",\
	.prev_uuid		= "UUID012_ALT_PREV",\
	.user_flags		= "012345USER_FLAGS",\
}

TEST_CASE_DECLARE(client_create);
TEST_CASE_DECLARE(client_open);
TEST_CASE_DECLARE(client_set_attr);
TEST_CASE_DECLARE(server);

/*
 * client_create -- perform create request
 */
int
client_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	int ret;
	struct rpmem_obc *rpc;
	struct rpmem_target_info *info;
	struct rpmem_req_attr req = REQ_ATTR_INIT;
	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;
	struct rpmem_resp_attr ex_res = RESP_ATTR_INIT;
	struct rpmem_resp_attr res;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	ret = rpmem_obc_connect(rpc, info);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_create(rpc, &req, &res, &pool_attr);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ex_res.port, res.port);
	UT_ASSERTeq(ex_res.rkey, res.rkey);
	UT_ASSERTeq(ex_res.raddr, res.raddr);
	UT_ASSERTeq(ex_res.persist_method, res.persist_method);
	UT_ASSERTeq(ex_res.nlanes, res.nlanes);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_close(rpc, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_obc_disconnect(rpc);
	UT_ASSERTeq(ret, 0);

	rpmem_obc_fini(rpc);

	return 1;
}

/*
 * client_open -- perform open request
 */
int
client_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	int ret;
	struct rpmem_obc *rpc;
	struct rpmem_target_info *info;
	struct rpmem_req_attr req = REQ_ATTR_INIT;
	struct rpmem_pool_attr ex_pool_attr = POOL_ATTR_INIT;
	struct rpmem_pool_attr pool_attr;
	struct rpmem_resp_attr ex_res = RESP_ATTR_INIT;
	struct rpmem_resp_attr res;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	ret = rpmem_obc_connect(rpc, info);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_open(rpc, &req, &res, &pool_attr);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ex_res.port, res.port);
	UT_ASSERTeq(ex_res.rkey, res.rkey);
	UT_ASSERTeq(ex_res.raddr, res.raddr);
	UT_ASSERTeq(ex_res.persist_method, res.persist_method);
	UT_ASSERTeq(ex_res.nlanes, res.nlanes);
	UT_ASSERTeq(memcmp(&ex_pool_attr, &pool_attr,
			sizeof(ex_pool_attr)), 0);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_close(rpc, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_obc_disconnect(rpc);
	UT_ASSERTeq(ret, 0);

	rpmem_obc_fini(rpc);

	return 1;
}

/*
 * client_set_attr -- perform set attributes request
 */
int
client_set_attr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	int ret;
	struct rpmem_obc *rpc;
	struct rpmem_target_info *info;
	const struct rpmem_pool_attr pool_attr = POOL_ATTR_ALT;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	ret = rpmem_obc_connect(rpc, info);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_set_attr(rpc, &pool_attr);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 1);

	ret = rpmem_obc_close(rpc, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_obc_disconnect(rpc);
	UT_ASSERTeq(ret, 0);

	rpmem_obc_fini(rpc);

	return 1;
}

/*
 * req_arg -- request callbacks argument
 */
struct req_arg {
	struct rpmem_resp_attr resp;
	struct rpmem_pool_attr pool_attr;
	int closing;
};

/*
 * req_create -- process create request
 */
static int
req_create(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr)
{
	struct rpmem_req_attr ex_req = REQ_ATTR_INIT;
	struct rpmem_pool_attr ex_pool_attr = POOL_ATTR_INIT;
	UT_ASSERTne(arg, NULL);
	UT_ASSERTeq(ex_req.provider, req->provider);
	UT_ASSERTeq(ex_req.pool_size, req->pool_size);
	UT_ASSERTeq(ex_req.nlanes, req->nlanes);
	UT_ASSERTeq(strcmp(ex_req.pool_desc, req->pool_desc), 0);
	UT_ASSERTeq(memcmp(&ex_pool_attr, pool_attr, sizeof(ex_pool_attr)), 0);

	struct req_arg *args = arg;

	return rpmemd_obc_create_resp(obc, 0, &args->resp);
}

/*
 * req_open -- process open request
 */
static int
req_open(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req)
{
	struct rpmem_req_attr ex_req = REQ_ATTR_INIT;
	UT_ASSERTne(arg, NULL);
	UT_ASSERTeq(ex_req.provider, req->provider);
	UT_ASSERTeq(ex_req.pool_size, req->pool_size);
	UT_ASSERTeq(ex_req.nlanes, req->nlanes);
	UT_ASSERTeq(strcmp(ex_req.pool_desc, req->pool_desc), 0);

	struct req_arg *args = arg;

	return rpmemd_obc_open_resp(obc, 0,
			&args->resp, &args->pool_attr);
}

/*
 * req_set_attr -- process set attributes request
 */
static int
req_set_attr(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_pool_attr *pool_attr)
{
	struct rpmem_pool_attr ex_pool_attr = POOL_ATTR_ALT;
	UT_ASSERTne(arg, NULL);
	UT_ASSERTeq(memcmp(&ex_pool_attr, pool_attr, sizeof(ex_pool_attr)), 0);

	return rpmemd_obc_set_attr_resp(obc, 0);
}

/*
 * req_close -- process close request
 */
static int
req_close(struct rpmemd_obc *obc, void *arg, int flags)
{
	UT_ASSERTne(arg, NULL);

	struct req_arg *args = arg;
	args->closing = 1;

	return rpmemd_obc_close_resp(obc, 0);
}

/*
 * REQ -- server request callbacks
 */
static struct rpmemd_obc_requests REQ = {
	.create = req_create,
	.open = req_open,
	.close = req_close,
	.set_attr = req_set_attr,
};

/*
 * server -- run server and process clients requests
 */
int
server(const struct test_case *tc, int argc, char *argv[])
{
	int ret;

	struct req_arg arg = {
		.resp = RESP_ATTR_INIT,
		.pool_attr = POOL_ATTR_INIT,
		.closing = 0,
	};

	struct rpmemd_obc *obc;

	obc = rpmemd_obc_init(0, 1);
	UT_ASSERTne(obc, NULL);

	ret = rpmemd_obc_status(obc, 0);
	UT_ASSERTeq(ret, 0);

	while (1) {
		ret = rpmemd_obc_process(obc, &REQ, &arg);
		if (arg.closing) {
			break;
		} else {
			UT_ASSERTeq(ret, 0);
		}
	}

	ret = rpmemd_obc_process(obc, &REQ, &arg);
	UT_ASSERTeq(ret, 1);

	rpmemd_obc_fini(obc);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(server),
	TEST_CASE(client_create),
	TEST_CASE(client_open),
	TEST_CASE(client_set_attr),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_obc");
	common_init("rpmem_fip",
		"RPMEM_LOG_LEVEL",
		"RPMEM_LOG_FILE", 0, 0);
	rpmemd_log_init("rpmemd", os_getenv("RPMEMD_LOG_FILE"), 0);
	rpmemd_log_level = rpmemd_log_level_from_str(
			os_getenv("RPMEMD_LOG_LEVEL"));
	rpmem_util_cmds_init();

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	rpmem_util_cmds_fini();
	common_fini();
	rpmemd_log_close();
	DONE(NULL);
}
