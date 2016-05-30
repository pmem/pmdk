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
 * rpmem_fip_test.c -- tests for rpmem_fip and rpmemd_fip modules
 */
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "unittest.h"
#include "out.h"

#include "librpmem.h"
#include "rpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_fip_common.h"
#include "rpmem_fip_sock.h"
#include "rpmemd_fip.h"
#include "rpmemd_log.h"
#include "rpmem_fip.h"

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
		UT_ASSERT(0);
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
		*nlanes = SOCK_NLANES;

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

	for (unsigned i = 0; i < COUNT_PER_LANE; i++) {
		size_t offset = args->lane * TOTAL_PER_LANE + i * SIZE_PER_LANE;
		unsigned val = args->lane + i;
		memset(&lpool[offset], val, SIZE_PER_LANE);

		ret = rpmem_fip_persist(args->fip, offset,
				SIZE_PER_LANE, args->lane);
		UT_ASSERTeq(ret, 0);
	}

	return NULL;
}

/*
 * client_init -- test case for client initialization
 */
void
client_init(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr>[:<port>] <provider>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];

	char *node;
	char *service;
	char fip_service[NI_MAXSERV];

	int ret;

	ret = rpmem_target_split(target, NULL, &node, &service);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(node, NULL);
	UT_ASSERTne(service, NULL);

	unsigned nlanes;
	enum rpmem_provider provider = get_provider(node,
			prov_name, &nlanes);

	int fd;
	struct rpmem_resp_attr resp;
	struct sockaddr_in addr_in;
	fd = client_exchange(node, service, NLANES, provider,
			&resp, &addr_in);

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
	fip = rpmem_fip_init(node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	client_close(fd);

	rpmem_fip_fini(fip);

	FREE(node);
	FREE(service);
}

/*
 * server_init -- test case for server initialization
 */
void
server_init(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s <addr> <port> <persist method>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	enum rpmem_persist_method persist_method = get_persist_method(argv[2]);

	int fd;
	unsigned nlanes;
	enum rpmem_provider provider;
	struct sockaddr_in addr_in;
	fd = server_exchange_begin(node, service, &nlanes,
			&provider, &addr_in);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.persist = pmem_persist,
		.nthreads = NTHREADS,
	};

	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	const char *addr = inet_ntoa(addr_in.sin_addr);
	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(fd, resp);
	server_close_begin(fd);
	server_close_end(fd);

	rpmemd_fip_fini(fip);
}

/*
 * client_connect -- test case for establishing connection - client side
 */
void
client_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr>[:<port>] <provider>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];

	char *node;
	char *service;
	char fip_service[NI_MAXSERV];

	int ret;

	ret = rpmem_target_split(target, NULL, &node, &service);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(node, NULL);
	UT_ASSERTne(service, NULL);

	unsigned nlanes;
	enum rpmem_provider provider = get_provider(node,
			prov_name, &nlanes);

	int fd;
	struct rpmem_resp_attr resp;
	struct sockaddr_in addr_in;
	fd = client_exchange(node, service, NLANES, provider,
			&resp, &addr_in);

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
	fip = rpmem_fip_init(node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	client_close(fd);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmem_fip_fini(fip);

	FREE(node);
	FREE(service);
}

/*
 * server_connect -- test case for establishing connection - server side
 */
void
server_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s <addr> <port> <persist method>", tc->name);

	char *node = argv[0];
	char *service = argv[1];
	enum rpmem_persist_method persist_method = get_persist_method(argv[2]);

	int fd;
	unsigned nlanes;
	enum rpmem_provider provider;
	struct sockaddr_in addr_in;
	fd = server_exchange_begin(node, service, &nlanes,
			&provider, &addr_in);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.persist = pmem_persist,
		.nthreads = NTHREADS,
	};

	int ret;
	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	const char *addr = inet_ntoa(addr_in.sin_addr);
	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(fd, resp);

	ret = rpmemd_fip_accept(fip);
	UT_ASSERTeq(ret, 0);

	server_close_begin(fd);
	server_close_end(fd);

	ret = rpmemd_fip_wait_close(fip, -1);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmemd_fip_fini(fip);
}

/*
 * server_process -- test case for processing data on server side
 */
void
server_process(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s <addr> <port> <persist method>", tc->name);

	char *node = argv[0];
	char *service = argv[1];
	enum rpmem_persist_method persist_method = get_persist_method(argv[2]);

	set_pool_data(rpool, 1);

	int fd;
	unsigned nlanes;
	enum rpmem_provider provider;
	struct sockaddr_in addr_in;
	fd = server_exchange_begin(node, service, &nlanes,
			&provider, &addr_in);

	struct rpmemd_fip_attr attr = {
		.addr = rpool,
		.size = POOL_SIZE,
		.nlanes = nlanes,
		.provider = provider,
		.persist_method = persist_method,
		.persist = pmem_persist,
		.nthreads = NTHREADS,
	};

	int ret;
	struct rpmem_resp_attr resp;
	struct rpmemd_fip *fip;
	enum rpmem_err err;

	const char *addr = inet_ntoa(addr_in.sin_addr);
	fip = rpmemd_fip_init(addr, NULL, &attr, &resp, &err);
	UT_ASSERTne(fip, NULL);

	server_exchange_end(fd, resp);

	ret = rpmemd_fip_accept(fip);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_process_start(fip);

	server_close_begin(fd);

	ret = rpmemd_fip_process_stop(fip);
	UT_ASSERTeq(ret, 0);

	server_close_end(fd);

	ret = rpmemd_fip_wait_close(fip, -1);
	UT_ASSERTeq(ret, 0);

	ret = rpmemd_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmemd_fip_fini(fip);
}

/*
 * client_persist -- test case for single-threaded persist operation
 */
void
client_persist(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr>[:<port>] <provider>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];

	char *node;
	char *service;
	char fip_service[NI_MAXSERV];

	int ret;

	ret = rpmem_target_split(target, NULL, &node, &service);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(node, NULL);
	UT_ASSERTne(service, NULL);

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	unsigned nlanes;
	enum rpmem_provider provider = get_provider(node,
			prov_name, &nlanes);

	int fd;
	struct rpmem_resp_attr resp;
	struct sockaddr_in addr_in;
	fd = client_exchange(node, service, NLANES, provider, &resp, &addr_in);

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
	fip = rpmem_fip_init(node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_start(fip);
	UT_ASSERTeq(ret, 0);

	struct persist_arg arg = {
		.fip = fip,
		.lane = 0,
	};

	client_persist_thread(&arg);

	ret = rpmem_fip_read(fip, rpool, POOL_SIZE, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_stop(fip);
	UT_ASSERTeq(ret, 0);

	client_close(fd);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmem_fip_fini(fip);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	FREE(node);
	FREE(service);
}

/*
 * client_persist_mt -- test case for multi-threaded persist operation
 */
void
client_persist_mt(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr>[:<port>] <provider>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];

	char *node;
	char *service;
	char fip_service[NI_MAXSERV];

	int ret;

	ret = rpmem_target_split(target, NULL, &node, &service);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(node, NULL);
	UT_ASSERTne(service, NULL);

	set_pool_data(lpool, 1);
	set_pool_data(rpool, 1);

	unsigned nlanes;
	enum rpmem_provider provider = get_provider(node,
			prov_name, &nlanes);

	int fd;
	struct rpmem_resp_attr resp;
	struct sockaddr_in addr_in;
	fd = client_exchange(node, service, NLANES, provider,
			&resp, &addr_in);

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
	fip = rpmem_fip_init(node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_start(fip);
	UT_ASSERTeq(ret, 0);

	pthread_t *persist_thread = MALLOC(resp.nlanes * sizeof(pthread_t));
	struct persist_arg *args = MALLOC(resp.nlanes *
			sizeof(struct persist_arg));

	for (unsigned i = 0; i < nlanes; i++) {
		args[i].fip = fip;
		args[i].lane = i;
		PTHREAD_CREATE(&persist_thread[i], NULL,
				client_persist_thread, &args[i]);
	}

	for (unsigned i = 0; i < nlanes; i++)
		PTHREAD_JOIN(persist_thread[i], NULL);

	ret = rpmem_fip_read(fip, rpool, POOL_SIZE, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_stop(fip);
	UT_ASSERTeq(ret, 0);

	client_close(fd);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmem_fip_fini(fip);

	FREE(persist_thread);
	FREE(args);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	FREE(node);
	FREE(service);
}

/*
 * client_read -- test case for read operation
 */
void
client_read(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr>[:<port>] <provider>", tc->name);

	char *target = argv[0];
	char *prov_name = argv[1];

	char *node;
	char *service;
	char fip_service[NI_MAXSERV];

	int ret;

	ret = rpmem_target_split(target, NULL, &node, &service);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(node, NULL);
	UT_ASSERTne(service, NULL);

	set_pool_data(lpool, 0);
	set_pool_data(rpool, 1);

	unsigned nlanes;
	enum rpmem_provider provider = get_provider(node,
			prov_name, &nlanes);

	int fd;
	struct rpmem_resp_attr resp;
	struct sockaddr_in addr_in;
	fd = client_exchange(node, service, NLANES, provider,
			&resp, &addr_in);

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
	fip = rpmem_fip_init(node, fip_service, &attr, &nlanes);
	UT_ASSERTne(fip, NULL);

	ret = rpmem_fip_connect(fip);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_start(fip);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_read(fip, lpool, POOL_SIZE, 0);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_fip_process_stop(fip);
	UT_ASSERTeq(ret, 0);

	client_close(fd);

	ret = rpmem_fip_close(fip);
	UT_ASSERTeq(ret, 0);

	rpmem_fip_fini(fip);

	ret = memcmp(rpool, lpool, POOL_SIZE);
	UT_ASSERTeq(ret, 0);

	FREE(node);
	FREE(service);
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
	rpmem_fip_probe_get(NULL, NULL);
	START(argc, argv, "rpmem_obc");
	out_init("rpmem_fip",
		"RPMEM_LOG_LEVEL",
		"RPMEM_LOG_FILE", 0, 0);
	rpmemd_log_init("rpmemd", getenv("RPMEMD_LOG_FILE"), 0);
	rpmemd_log_level = rpmemd_log_level_from_str(
			getenv("RPMEMD_LOG_LEVEL"));
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	out_fini();
	rpmemd_log_close();
	DONE(NULL);
}
