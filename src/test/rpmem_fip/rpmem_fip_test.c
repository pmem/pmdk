/*
 * Copyright 2016-2018, Intel Corporation
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
 * rpmem_fip_test.c -- tests for rpmem_fip and rpmemd_fip modules
 */
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "unittest.h"
#include "pmemcommon.h"

#include "librpmem.h"
#include "rpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_fip_common.h"
#include "rpmem_fip_oob.h"
#include "rpmemd_fip.h"
#include "rpmemd_log.h"
#include "rpmemd_util.h"
#include "rpmem_fip.h"
#include "os.h"

#define SIZE_PER_LANE	64
#define COUNT_PER_LANE	32
#define NLANES		1024
#define SOCK_NLANES	32
#define NTHREADS	32
#define TOTAL_PER_LANE	(SIZE_PER_LANE * COUNT_PER_LANE)
#define POOL_SIZE	(NLANES * TOTAL_PER_LANE)

uint8_t lpool[POOL_SIZE];
uint8_t rpool[POOL_SIZE];

TEST_CASE_DECLARE(client_init);
TEST_CASE_DECLARE(server_init);
TEST_CASE_DECLARE(client_connect);
TEST_CASE_DECLARE(server_connect);
TEST_CASE_DECLARE(server_process);
TEST_CASE_DECLARE(client_persist);
TEST_CASE_DECLARE(client_persist_mt);
TEST_CASE_DECLARE(client_read);

/*
 * get_persist_method -- parse persist method
 */
static enum rpmem_persist_method
get_persist_method(const char *pm)
{
	if (strcmp(pm, "GPSPM") == 0)
		return RPMEM_PM_GPSPM;
	else if (strcmp(pm, "APM") == 0)
		return RPMEM_PM_APM;
	else
		UT_FATAL("unknown method");
}

/*
 * get_provider -- get provider for given target
 */
static enum rpmem_provider
get_provider(const char *target, const char *prov_name, unsigned *nlanes)
{
	struct rpmem_fip_probe probe;
	int ret;

	int any = 0;
	if (strcmp(prov_name, "any") == 0)
		any = 1;

	ret = rpmem_fip_probe_get(target, &probe);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(rpmem_fip_probe_any(probe));

	enum rpmem_provider provider;
	if (any) {
		/* return verbs in first place */
		if (rpmem_fip_probe(probe,
				RPMEM_PROV_LIBFABRIC_VERBS))
			provider = RPMEM_PROV_LIBFABRIC_VERBS;
		else if (rpmem_fip_probe(probe,
				RPMEM_PROV_LIBFABRIC_SOCKETS))
			provider = RPMEM_PROV_LIBFABRIC_SOCKETS;
		else
			UT_ASSERT(0);
	} else {
		provider = rpmem_provider_from_str(prov_name);
		UT_ASSERTne(provider, RPMEM_PROV_UNKNOWN);
		UT_ASSERT(rpmem_fip_probe(probe, provider));
	}

	/*
	 * Decrease number of lanes for socket provider because
	 * the test may be too long.
	 */
	if (provider == RPMEM_PROV_LIBFABRIC_SOCKETS)
		*nlanes = min(*nlanes, SOCK_NLANES);

	return provider;
}

/*
 * set_pool_data -- set pools data to well known values
 */
static void
set_pool_data(uint8_t *pool, int inverse)
{
	for (unsigned l = 0; l < NLANES; l++) {
		for (unsigned i = 0; i < COUNT_PER_LANE; i++) {
			size_t offset = l * TOTAL_PER_LANE + i * SIZE_PER_LANE;
			unsigned val = i + l;
			if (inverse)
				val = ~val;

			memset(&pool[offset], (int)val, SIZE_PER_LANE);
		}
	}
}

/*
 * persist_arg -- arguments for client persist thread
 */
struct persist_arg {
	struct rpmem_fip *fip;
	unsigned lane;
};

/*
 * client_persist_thread -- thread callback for persist operation
 */
static void *
client_persist_thread(void *arg)
{
	struct persist_arg *args = arg;
	int ret;

	/* presist with len == 0 should always succeed */
	ret = rpmem_fip_persist(args->fip, args->lane * TOTAL_PER_LANE,
			0, args->lane, RPMEM_PERSIST);
	UT_ASSERTeq(ret, 0);

	for (unsigned i = 0; i < COUNT_PER_LANE; i++) {
		size_t offset = args->lane * TOTAL_PER_LANE + i * SIZE_PER_LANE;
		unsigned val = args->lane + i;
		memset(&lpool[offset], val, SIZE_PER_LANE);

		ret = rpmem_fip_persist(args->fip, offset,
				SIZE_PER_LANE, args->lane, RPMEM_PERSIST);
		UT_ASSERTeq(ret, 0);
	}

	return NULL;
}

/*
 * client_init -- test case for client initialization
 */
int
client_init(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	set_rpmem_cmd("server_init %s", persist_method);

	char fip_service[NI_MAXSERV];

	struct rpmem_target_info *info;
	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	unsigned nlanes = NLANES;
	enum rpmem_provider provider = get_provider(info->node,
			prov_name, &nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, nlanes, provider, &resp);

	struct rpmem_fip_attr attr = {
		.provider = provider,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = snprintf(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	/*
	 * Tune the maximum number of lanes according to environment.
	 */
	rpmem_util_get_env_max_nlanes(&Rpmem_max_nlanes);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	client_close_begin(client);
	client_close_end(client);

	rpmem_fip_fini(fip);
	rpmem_target_free(info);

	return 3;
}

/*
 * server_init -- test case for server initialization
 */
int
server_init(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <persist method>", tc->name);

	enum rpmem_persist_method persist_method = get_persist_method(argv[0]);

	unsigned nlanes;
	enum rpmem_provider provider;
	char *addr = NULL;
	int ret;
	server_exchange_begin(&nlanes, &provider, &addr);
	UT_ASSERTne(addr, NULL);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.nthreads = NTHREADS,
	};

	ret = rpmemd_apply_pm_policy(&attr.persist_method, &attr.persist,
			1 /* is pmem */);
	UT_ASSERTeq(ret, 0);

	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(resp);
	server_close_begin();
	server_close_end();

	rpmemd_fip_fini(fip);

	FREE(addr);

	return 1;
}

/*
 * client_connect -- test case for establishing connection - client side
 */
int
client_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	set_rpmem_cmd("server_connect %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;
	int ret;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	unsigned nlanes = NLANES;
	enum rpmem_provider provider = get_provider(info->node,
			prov_name, &nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, nlanes, provider, &resp);

	struct rpmem_fip_attr attr = {
		.provider = provider,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = snprintf(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);
	rpmem_target_free(info);

	return 3;
}

/*
 * server_connect -- test case for establishing connection - server side
 */
int
server_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <persist method>", tc->name);

	enum rpmem_persist_method persist_method = get_persist_method(argv[0]);

	unsigned nlanes;
	enum rpmem_provider provider;
	char *addr = NULL;
	server_exchange_begin(&nlanes, &provider, &addr);
	UT_ASSERTne(addr, NULL);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.nthreads = NTHREADS,
	};

	int ret;
	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	ret = rpmemd_apply_pm_policy(&attr.persist_method, &attr.persist,
			1 /* is pmem */);
	UT_ASSERTeq(ret, 0);

	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(resp);

	ret = rpmemd_fip_accept(fip, -1);
	UT_ASSERTeq(ret, 0);

	server_close_begin();
	server_close_end();

	ret = rpmemd_fip_wait_close(fip, -1);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmemd_fip_fini(fip);

	FREE(addr);

	return 1;
}

/*
 * server_process -- test case for processing data on server side
 */
int
server_process(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <persist method>", tc->name);

	enum rpmem_persist_method persist_method = get_persist_method(argv[0]);

	set_pool_data(rpool, 1);

	unsigned nlanes;
	enum rpmem_provider provider;
	char *addr = NULL;
	server_exchange_begin(&nlanes, &provider, &addr);
	UT_ASSERTne(addr, NULL);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.nthreads = NTHREADS,
	};

	int ret;
	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	ret = rpmemd_apply_pm_policy(&attr.persist_method, &attr.persist,
			1 /* is pmem */);
	UT_ASSERTeq(ret, 0);

	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(resp);

	ret = rpmemd_fip_accept(fip, -1);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_process_start(fip);

	server_close_begin();

	ret = rpmemd_fip_process_stop(fip);
	UT_ASSERTeq(ret, 0);

	server_close_end();

	ret = rpmemd_fip_wait_close(fip, -1);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmemd_fip_fini(fip);

	FREE(addr);

	return 1;
}

/*
 * client_persist -- test case for single-threaded persist operation
 */
int
client_persist(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	int ret;

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	unsigned nlanes = NLANES;
	enum rpmem_provider provider = get_provider(info->node,
			prov_name, &nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, nlanes, provider, &resp);

	struct rpmem_fip_attr attr = {
		.provider = provider,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = snprintf(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	struct persist_arg arg = {
		.fip = fip,
		.lane = 0,
	};

	client_persist_thread(&arg);

	ret = rpmem_fip_read(fip, rpool, POOL_SIZE, 0, 0);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	return 3;
}

/*
 * client_persist_mt -- test case for multi-threaded persist operation
 */
int
client_persist_mt(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;
	int ret;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	unsigned nlanes = NLANES;
	enum rpmem_provider provider = get_provider(info->node,
			prov_name, &nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, nlanes, provider, &resp);

	struct rpmem_fip_attr attr = {
		.provider = provider,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = snprintf(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	os_thread_t *persist_thread = MALLOC(resp.nlanes * sizeof(os_thread_t));
	struct persist_arg *args = MALLOC(resp.nlanes *
			sizeof(struct persist_arg));

	for (unsigned i = 0; i < nlanes; i++) {
		args[i].fip = fip;
		args[i].lane = i;
		PTHREAD_CREATE(&persist_thread[i], NULL,
				client_persist_thread, &args[i]);
	}

	for (unsigned i = 0; i < nlanes; i++)
		PTHREAD_JOIN(&persist_thread[i], NULL);

	ret = rpmem_fip_read(fip, rpool, POOL_SIZE, 0, 0);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);

	FREE(persist_thread);
	FREE(args);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	return 3;
}

/*
 * client_read -- test case for read operation
 */
int
client_read(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;
	int ret;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	set_pool_data(lpool, 0);
	set_pool_data(rpool, 1);

	unsigned nlanes = NLANES;
	enum rpmem_provider provider = get_provider(info->node,
			prov_name, &nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, nlanes, provider, &resp);

	struct rpmem_fip_attr attr = {
		.provider = provider,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = snprintf(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	/* read with len == 0 should always succeed */
	ret = rpmem_fip_read(fip, lpool, 0, 0, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_read(fip, lpool, POOL_SIZE, 0, 0);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	return 3;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(client_init),
	TEST_CASE(server_init),
	TEST_CASE(client_connect),
	TEST_CASE(server_connect),
	TEST_CASE(client_persist),
	TEST_CASE(client_persist_mt),
	TEST_CASE(server_process),
	TEST_CASE(client_read),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	/* workaround for left-opened files by libfabric */
	rpmem_fip_probe_get("localhost", NULL);
	START(argc, argv, "rpmem_obc");
	common_init("rpmem_fip",
		"RPMEM_LOG_LEVEL",
		"RPMEM_LOG_FILE", 0, 0);
	rpmem_util_cmds_init();
	rpmemd_log_init("rpmemd", os_getenv("RPMEMD_LOG_FILE"), 0);
	rpmemd_log_level = rpmemd_log_level_from_str(
			os_getenv("RPMEMD_LOG_LEVEL"));
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	common_fini();
	rpmemd_log_close();
	rpmem_util_cmds_fini();
	DONE(NULL);
}
