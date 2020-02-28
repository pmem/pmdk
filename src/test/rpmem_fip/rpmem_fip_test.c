// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

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

static uint8_t lpool[POOL_SIZE];
static uint8_t rpool[POOL_SIZE];

TEST_CASE_DECLARE(client_init);
TEST_CASE_DECLARE(server_init);
TEST_CASE_DECLARE(client_connect);
TEST_CASE_DECLARE(server_connect);
TEST_CASE_DECLARE(server_process);
TEST_CASE_DECLARE(client_flush);
TEST_CASE_DECLARE(client_flush_mt);
TEST_CASE_DECLARE(client_persist);
TEST_CASE_DECLARE(client_persist_mt);
TEST_CASE_DECLARE(client_read);
TEST_CASE_DECLARE(client_wq_size);

struct fip_client {
	enum rpmem_provider provider;
	unsigned max_wq_size;
	unsigned nlanes;
};

#define FIP_CLIENT_DEFAULT {RPMEM_PROV_UNKNOWN, 0, NLANES}

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
static void
get_provider(const char *target, const char *prov_name,
	struct fip_client *client)
{
	struct rpmem_fip_probe probe;
	int ret;

	int any = 0;
	if (strcmp(prov_name, "any") == 0)
		any = 1;

	ret = rpmem_fip_probe_get(target, &probe);
	UT_ASSERTeq(ret, 0);
	UT_ASSERT(rpmem_fip_probe_any(probe));

	if (any) {
		/* return verbs in first place */
		if (rpmem_fip_probe(probe,
				RPMEM_PROV_LIBFABRIC_VERBS))
			client->provider = RPMEM_PROV_LIBFABRIC_VERBS;
		else if (rpmem_fip_probe(probe,
				RPMEM_PROV_LIBFABRIC_SOCKETS))
			client->provider = RPMEM_PROV_LIBFABRIC_SOCKETS;
		else
			UT_ASSERT(0);
	} else {
		client->provider = rpmem_provider_from_str(prov_name);
		UT_ASSERTne(client->provider, RPMEM_PROV_UNKNOWN);
		UT_ASSERT(rpmem_fip_probe(probe, client->provider));
	}

	/*
	 * Decrease number of lanes for socket provider because
	 * the test may be too long.
	 */
	if (client->provider == RPMEM_PROV_LIBFABRIC_SOCKETS)
		client->nlanes = min(client->nlanes, SOCK_NLANES);

	client->max_wq_size = probe.max_wq_size[client->provider];
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
 * flush_arg -- arguments for client persist and flush / drain threads
 */
struct flush_arg {
	struct rpmem_fip *fip;
	unsigned lane;
};

typedef void *(*flush_fn)(void *arg);

/*
 * client_flush_thread -- thread callback for flush / drain operation
 */
static void *
client_flush_thread(void *arg)
{
	struct flush_arg *args = arg;
	int ret;

	/* persist with len == 0 should always succeed */
	ret = rpmem_fip_flush(args->fip, args->lane * TOTAL_PER_LANE,
			0, args->lane, RPMEM_FLUSH_WRITE);
	UT_ASSERTeq(ret, 0);

	for (unsigned i = 0; i < COUNT_PER_LANE; i++) {
		size_t offset = args->lane * TOTAL_PER_LANE + i * SIZE_PER_LANE;
		unsigned val = args->lane + i;
		memset(&lpool[offset], (int)val, SIZE_PER_LANE);

		ret = rpmem_fip_flush(args->fip, offset,
				SIZE_PER_LANE, args->lane, RPMEM_FLUSH_WRITE);
		UT_ASSERTeq(ret, 0);
	}

	ret = rpmem_fip_drain(args->fip, args->lane);
	UT_ASSERTeq(ret, 0);

	return NULL;
}

/*
 * client_persist_thread -- thread callback for persist operation
 */
static void *
client_persist_thread(void *arg)
{
	struct flush_arg *args = arg;
	int ret;

	/* persist with len == 0 should always succeed */
	ret = rpmem_fip_persist(args->fip, args->lane * TOTAL_PER_LANE,
			0, args->lane, RPMEM_FLUSH_WRITE);
	UT_ASSERTeq(ret, 0);

	for (unsigned i = 0; i < COUNT_PER_LANE; i++) {
		size_t offset = args->lane * TOTAL_PER_LANE + i * SIZE_PER_LANE;
		unsigned val = args->lane + i;
		memset(&lpool[offset], (int)val, SIZE_PER_LANE);

		ret = rpmem_fip_persist(args->fip, offset,
				SIZE_PER_LANE, args->lane, RPMEM_FLUSH_WRITE);
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

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	/*
	 * Tune the maximum number of lanes according to environment.
	 */
	rpmem_util_get_env_max_nlanes(&Rpmem_max_nlanes);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
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
			&attr.memcpy_persist,
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

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
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
			&attr.memcpy_persist,
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
			&attr.memcpy_persist,
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
 * flush_common -- common part for single-threaded persist and flush / drain
 * test cases
 */
static void
flush_common(char *target, char *prov_name, char *persist_method,
		flush_fn flush_func)
{
	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	int ret;

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	struct flush_arg arg = {
		.fip = fip,
		.lane = 0,
	};

	flush_func(&arg);

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
}

/*
 * flush_common_mt -- common part for multi-threaded persist and flush / drain
 * test cases
 */
static int
flush_common_mt(char *target, char *prov_name, char *persist_method,
		flush_fn flush_thread_func)
{
	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;
	int ret;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	os_thread_t *flush_thread = MALLOC(resp.nlanes * sizeof(os_thread_t));
	struct flush_arg *args = MALLOC(resp.nlanes * sizeof(struct flush_arg));

	for (unsigned i = 0; i < fip_client.nlanes; i++) {
		args[i].fip = fip;
		args[i].lane = i;
		THREAD_CREATE(&flush_thread[i], NULL,
				flush_thread_func, &args[i]);
	}

	for (unsigned i = 0; i < fip_client.nlanes; i++)
		THREAD_JOIN(&flush_thread[i], NULL);

	ret = rpmem_fip_read(fip, rpool, POOL_SIZE, 0, 0);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);

	FREE(flush_thread);
	FREE(args);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	rpmem_target_free(info);

	return 3;
}

/*
 * client_flush -- test case for single-threaded flush / drain operation
 */
int
client_flush(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	flush_common(target, prov_name, persist_method, client_flush_thread);

	return 3;
}

/*
 * client_flush_mt -- test case for multi-threaded flush / drain operation
 */
int
client_flush_mt(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>",
				tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];

	flush_common_mt(target, prov_name, persist_method, client_flush_thread);

	return 3;
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

	flush_common(target, prov_name, persist_method, client_persist_thread);

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

	flush_common_mt(target, prov_name, persist_method,
			client_persist_thread);

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

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
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

#define LT_MAX_WQ_SIZE "LT_MAX_WQ_SIZE" /* < max_wq_size */
#define EQ_MAX_WQ_SIZE "EQ_MAX_WQ_SIZE" /* == max_wq_size */
#define GT_MAX_WQ_SIZE "GT_MAX_WQ_SIZE" /* > max_wq_size */

/*
 * client_wq_size -- test case for WQ size adjustment
 */
int
client_wq_size(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL("usage: %s <target> <provider> <persist method>"
				"<wq_size>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];
	char *persist_method = argv[2];
	char *wq_size_env_str = argv[3];

	set_rpmem_cmd("server_process %s", persist_method);

	char fip_service[NI_MAXSERV];
	struct rpmem_target_info *info;
	int ret;

	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	struct fip_client fip_client = FIP_CLIENT_DEFAULT;
	get_provider(info->node, prov_name, &fip_client);
	rpmem_util_get_env_max_nlanes(&fip_client.nlanes);

	client_t *client;
	struct rpmem_resp_attr resp;
	client = client_exchange(info, fip_client.nlanes, fip_client.provider,
			&resp);

	struct rpmem_fip_attr attr = {
		.provider = fip_client.provider,
		.max_wq_size = fip_client.max_wq_size,
		.persist_method = resp.persist_method,
		.laddr = lpool,
		.size = POOL_SIZE,
		.nlanes = resp.nlanes,
		.raddr = (void *)resp.raddr,
		.rkey = resp.rkey,
	};

	ssize_t sret = SNPRINTF(fip_service, NI_MAXSERV, "%u", resp.port);
	UT_ASSERT(sret > 0);

	/* check RPMEM_WORK_QUEUE_SIZE env processing */
	unsigned wq_size_default = Rpmem_wq_size;
	if (strcmp(wq_size_env_str, LT_MAX_WQ_SIZE) == 0) {
		Rpmem_wq_size = fip_client.max_wq_size - 1;
	} else if (strcmp(wq_size_env_str, EQ_MAX_WQ_SIZE) == 0) {
		Rpmem_wq_size = fip_client.max_wq_size;
	} else if (strcmp(wq_size_env_str, GT_MAX_WQ_SIZE) == 0) {
		Rpmem_wq_size = fip_client.max_wq_size + 1;
	} else {
		long wq_size_env = STRTOL(wq_size_env_str, NULL, 10);
		rpmem_util_get_env_wq_size(&Rpmem_wq_size);
		if (wq_size_env > 0) {
			if (wq_size_env < UINT_MAX)
				UT_ASSERT(Rpmem_wq_size == wq_size_env);
			else
				UT_ASSERT(Rpmem_wq_size == UINT_MAX);
		} else
			UT_ASSERT(Rpmem_wq_size == wq_size_default);
	}

	struct rpmem_fip *fip;
	fip = rpmem_fip_init(info->node, fip_service, &attr,
			&fip_client.nlanes);
	UT_ASSERTne(fip, NULL);

	size_t req_wq_size = rpmem_fip_wq_size(
			resp.persist_method, RPMEM_FIP_NODE_CLIENT);
	size_t eff_wq_size = rpmem_fip_get_wq_size(fip);

	/* max supported meets minimal requirements */
	UT_ASSERT(fip_client.max_wq_size >= req_wq_size);
	/* calculated meets minimal requirements */
	UT_ASSERT(eff_wq_size >= req_wq_size);
	/* calculated is supported */
	UT_ASSERT(eff_wq_size <= fip_client.max_wq_size);

	/* if forced by env meets minimal requirements */
	if (Rpmem_wq_size > req_wq_size) {
		/* and it is supported */
		if (Rpmem_wq_size <= fip_client.max_wq_size) {
			/* calculated is >= to forced */
			UT_ASSERT(eff_wq_size >= Rpmem_wq_size);
		} else {
			/* calculated is clipped to max supported */
			UT_ASSERT(eff_wq_size == fip_client.max_wq_size);
		}
	}

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	client_close_begin(client);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	client_close_end(client);

	rpmem_fip_fini(fip);

	rpmem_target_free(info);

	return 4;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(client_init),
	TEST_CASE(server_init),
	TEST_CASE(client_connect),
	TEST_CASE(server_connect),
	TEST_CASE(client_flush),
	TEST_CASE(client_flush_mt),
	TEST_CASE(client_persist),
	TEST_CASE(client_persist_mt),
	TEST_CASE(server_process),
	TEST_CASE(client_read),
	TEST_CASE(client_wq_size)
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
