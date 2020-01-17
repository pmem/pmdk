/*
 * Copyright 2016-2020, Intel Corporation
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
 * rpmemd.c -- rpmemd main source file
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "librpmem.h"
#include "rpmemd.h"
#include "rpmemd_log.h"
#include "rpmemd_config.h"
#include "rpmem_common.h"
#include "rpmemd_fip.h"
#include "rpmemd_obc.h"
#include "rpmemd_db.h"
#include "rpmemd_util.h"
#include "pool_hdr.h"
#include "os.h"
#include "os_thread.h"
#include "util.h"
#include "uuid.h"
#include "set.h"

/*
 * rpmemd -- rpmem handle
 */
struct rpmemd {
	struct rpmemd_obc *obc;	/* out-of-band connection handle */
	struct rpmemd_db *db;	/* pool set database handle */
	struct rpmemd_db_pool *pool; /* pool handle */
	char *pool_desc;	/* pool descriptor */
	struct rpmemd_fip *fip;	/* fabric provider handle */
	struct rpmemd_config config; /* configuration */
	enum rpmem_persist_method persist_method;
	int closing;		/* set when closing connection */
	int created;		/* pool created */
	os_thread_t fip_thread;
	int fip_running;
};

#ifdef DEBUG
/*
 * bool2str -- convert bool to yes/no string
 */
static inline const char *
bool2str(int v)
{
	return v ? "yes" : "no";
}
#endif

/*
 * str_or_null -- return null string instead of NULL pointer
 */
static inline const char *
_str(const char *str)
{
	if (!str)
		return "(null)";
	return str;
}

/*
 * uuid2str -- convert uuid to string
 */
static const char *
uuid2str(const uuid_t uuid)
{
	static char uuid_str[64] = {0, };

	int ret = util_uuid_to_string(uuid, uuid_str);
	if (ret != 0) {
		return "(error)";
	}

	return uuid_str;
}

/*
 * rpmemd_get_pm -- returns persist method based on configuration
 */
static enum rpmem_persist_method
rpmemd_get_pm(struct rpmemd_config *config)
{
	enum rpmem_persist_method ret = RPMEM_PM_GPSPM;

	if (config->persist_apm)
		ret = RPMEM_PM_APM;

	return ret;
}

/*
 * rpmemd_db_get_status -- convert error number to status for db operation
 */
static int
rpmemd_db_get_status(int err)
{
	switch (err) {
	case EEXIST:
		return RPMEM_ERR_EXISTS;
	case EACCES:
		return RPMEM_ERR_NOACCESS;
	case ENOENT:
		return RPMEM_ERR_NOEXIST;
	case EWOULDBLOCK:
		return RPMEM_ERR_BUSY;
	case EBADF:
		return RPMEM_ERR_BADNAME;
	case EINVAL:
		return RPMEM_ERR_POOL_CFG;
	default:
		return RPMEM_ERR_FATAL;
	}
}

/*
 * rpmemd_check_pool -- verify pool parameters
 */
static int
rpmemd_check_pool(struct rpmemd *rpmemd, const struct rpmem_req_attr *req,
	int *status)
{
	if (rpmemd->pool->pool_size < RPMEM_MIN_POOL) {
		RPMEMD_LOG(ERR, "invalid pool size -- must be >= %zu",
				RPMEM_MIN_POOL);
		*status = RPMEM_ERR_POOL_CFG;
		return -1;
	}

	if (rpmemd->pool->pool_size < req->pool_size) {
		RPMEMD_LOG(ERR, "requested size is too big");
		*status = RPMEM_ERR_BADSIZE;
		return -1;
	}

	return 0;
}

/*
 * rpmemd_deep_persist -- perform deep persist operation
 */
static int
rpmemd_deep_persist(const void *addr, size_t size, void *ctx)
{
	struct rpmemd *rpmemd = (struct rpmemd *)ctx;
	return util_replica_deep_persist(addr, size, rpmemd->pool->set, 0);
}

/*
 * rpmemd_common_fip_init -- initialize fabric provider
 */
static int
rpmemd_common_fip_init(struct rpmemd *rpmemd, const struct rpmem_req_attr *req,
	struct rpmem_resp_attr *resp, int *status)
{
	/* register the whole pool with header in RDMA */
	void *addr = (void *)((uintptr_t)rpmemd->pool->pool_addr);
	struct rpmemd_fip_attr fip_attr = {
		.addr		= addr,
		.size		= req->pool_size,
		.nlanes		= req->nlanes,
		.nthreads	= rpmemd->config.nthreads,
		.provider	= req->provider,
		.persist_method = rpmemd->persist_method,
		.deep_persist	= rpmemd_deep_persist,
		.ctx		= rpmemd,
		.buff_size	= req->buff_size,
	};

	const int is_pmem = rpmemd_db_pool_is_pmem(rpmemd->pool);
	if (rpmemd_apply_pm_policy(&fip_attr.persist_method,
			&fip_attr.persist,
			&fip_attr.memcpy_persist,
			is_pmem)) {
		*status = RPMEM_ERR_FATAL;
		goto err_fip_init;
	}

	const char *node = rpmem_get_ssh_conn_addr();
	enum rpmem_err err;

	rpmemd->fip = rpmemd_fip_init(node, NULL, &fip_attr, resp, &err);
	if (!rpmemd->fip) {
		*status = (int)err;
		goto err_fip_init;
	}

	return 0;
err_fip_init:
	return -1;
}

/*
 * rpmemd_print_req_attr -- print request attributes
 */
static void
rpmemd_print_req_attr(const struct rpmem_req_attr *req)
{
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "pool descriptor: '%s'",
			_str(req->pool_desc));
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "pool size: %lu", req->pool_size);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "nlanes: %u", req->nlanes);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "provider: %s",
			rpmem_provider_to_str(req->provider));
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "buff_size: %lu", req->buff_size);
}

/*
 * rpmemd_print_pool_attr -- print pool attributes
 */
static void
rpmemd_print_pool_attr(const struct rpmem_pool_attr *attr)
{
	if (attr == NULL) {
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "NULL");
	} else {
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "signature: '%s'",
				_str(attr->signature));
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "major: %u", attr->major);
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "compat_features: 0x%x",
				attr->compat_features);
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "incompat_features: 0x%x",
				attr->incompat_features);
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "ro_compat_features: 0x%x",
				attr->ro_compat_features);
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "poolset_uuid: %s",
				uuid2str(attr->poolset_uuid));
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "uuid: %s",
				uuid2str(attr->uuid));
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "next_uuid: %s",
				uuid2str(attr->next_uuid));
		RPMEMD_LOG(INFO, RPMEMD_LOG_INDENT "prev_uuid: %s",
			uuid2str(attr->prev_uuid));
	}
}

/*
 * rpmemd_print_resp_attr -- print response attributes
 */
static void
rpmemd_print_resp_attr(const struct rpmem_resp_attr *attr)
{
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "port: %u", attr->port);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "rkey: 0x%lx", attr->rkey);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "raddr: 0x%lx", attr->raddr);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "nlanes: %u", attr->nlanes);
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "persist method: %s",
			rpmem_persist_method_to_str(attr->persist_method));
}

/*
 * rpmemd_fip_thread -- background thread for establishing in-band connection
 */
static void *
rpmemd_fip_thread(void *arg)
{
	struct rpmemd *rpmemd = (struct rpmemd *)arg;
	int ret;

	RPMEMD_LOG(INFO, "waiting for in-band connection");

	ret = rpmemd_fip_accept(rpmemd->fip, RPMEM_ACCEPT_TIMEOUT);
	if (ret)
		goto err_accept;

	RPMEMD_LOG(NOTICE, "in-band connection established");

	ret = rpmemd_fip_process_start(rpmemd->fip);
	if (ret)
		goto err_process_start;

	return NULL;
err_process_start:
	rpmemd_fip_close(rpmemd->fip);
err_accept:
	return (void *)(uintptr_t)ret;
}

/*
 * rpmemd_fip_start_thread -- start background thread for establishing
 * in-band connection
 */
static int
rpmemd_fip_start_thread(struct rpmemd *rpmemd)
{
	errno = os_thread_create(&rpmemd->fip_thread, NULL,
			rpmemd_fip_thread, rpmemd);
	if (errno) {
		RPMEMD_LOG(ERR, "!creating in-band thread");
		goto err_os_thread_create;
	}

	rpmemd->fip_running = 1;

	return 0;
err_os_thread_create:
	return -1;
}

/*
 * rpmemd_fip_stop_thread -- stop background thread for in-band connection
 */
static int
rpmemd_fip_stop_thread(struct rpmemd *rpmemd)
{
	RPMEMD_ASSERT(rpmemd->fip_running);
	void *tret;
	errno = os_thread_join(&rpmemd->fip_thread, &tret);
	if (errno)
		RPMEMD_LOG(ERR, "!waiting for in-band thread");

	int ret = (int)(uintptr_t)tret;
	if (ret)
		RPMEMD_LOG(ERR, "in-band thread failed -- '%d'", ret);

	return ret;
}

/*
 * rpmemd_fip-stop -- stop in-band thread and stop processing thread
 */
static int
rpmemd_fip_stop(struct rpmemd *rpmemd)
{
	int ret;

	int fip_ret = rpmemd_fip_stop_thread(rpmemd);
	if (fip_ret) {
		RPMEMD_LOG(ERR, "!in-band thread failed");
	}

	if (!fip_ret) {
		ret = rpmemd_fip_process_stop(rpmemd->fip);
		if (ret) {
			RPMEMD_LOG(ERR, "!stopping fip process failed");
		}
	}

	rpmemd->fip_running = 0;

	return fip_ret;
}

/*
 * rpmemd_close_pool -- close pool and remove it if required
 */
static int
rpmemd_close_pool(struct rpmemd *rpmemd, int remove)
{
	int ret = 0;

	RPMEMD_LOG(NOTICE, "closing pool");
	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);
	RPMEMD_LOG(INFO, "pool closed");

	if (remove) {
		RPMEMD_LOG(NOTICE, "removing '%s'", rpmemd->pool_desc);
		ret = rpmemd_db_pool_remove(rpmemd->db,
				rpmemd->pool_desc, 0, 0);
		if (ret) {
			RPMEMD_LOG(ERR, "!removing pool '%s' failed",
					rpmemd->pool_desc);
		} else {
			RPMEMD_LOG(INFO, "removed '%s'", rpmemd->pool_desc);
		}
	}

	free(rpmemd->pool_desc);

	return ret;
}

/*
 * rpmemd_req_cleanup -- cleanup in-band connection and all resources allocated
 * during open/create requests
 */
static void
rpmemd_req_cleanup(struct rpmemd *rpmemd)
{
	if (!rpmemd->fip_running)
		return;

	int ret;

	ret = rpmemd_fip_stop(rpmemd);
	if (!ret) {
		rpmemd_fip_close(rpmemd->fip);
		rpmemd_fip_fini(rpmemd->fip);
	}

	int remove = rpmemd->created && ret;
	rpmemd_close_pool(rpmemd, remove);
}

/*
 * rpmemd_req_create -- handle create request
 */
static int
rpmemd_req_create(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "create request:");
	rpmemd_print_req_attr(req);
	RPMEMD_LOG(NOTICE, "pool attributes:");
	rpmemd_print_pool_attr(pool_attr);

	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	int ret;
	int status = 0;
	int err_send = 1;
	struct rpmem_resp_attr resp;
	memset(&resp, 0, sizeof(resp));

	if (rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool already opened");
		ret = -1;
		status = RPMEM_ERR_FATAL;
		goto err_pool_opened;
	}

	rpmemd->pool_desc = strdup(req->pool_desc);
	if (!rpmemd->pool_desc) {
		RPMEMD_LOG(ERR, "!allocating pool descriptor");
		ret = -1;
		status = RPMEM_ERR_FATAL;
		goto err_strdup;
	}

	rpmemd->pool = rpmemd_db_pool_create(rpmemd->db,
			req->pool_desc, 0, pool_attr);
	if (!rpmemd->pool) {
		ret = -1;
		status = rpmemd_db_get_status(errno);
		goto err_pool_create;
	}

	rpmemd->created = 1;

	ret = rpmemd_check_pool(rpmemd, req, &status);
	if (ret)
		goto err_pool_check;

	ret = rpmemd_common_fip_init(rpmemd, req, &resp, &status);
	if (ret)
		goto err_fip_init;

	RPMEMD_LOG(NOTICE, "create request response: (status = %u)", status);
	if (!status)
		rpmemd_print_resp_attr(&resp);
	ret = rpmemd_obc_create_resp(obc, status, &resp);
	if (ret)
		goto err_create_resp;

	ret = rpmemd_fip_start_thread(rpmemd);
	if (ret)
		goto err_fip_start;

	return 0;
err_fip_start:
err_create_resp:
	err_send = 0;
	rpmemd_fip_fini(rpmemd->fip);
err_fip_init:
err_pool_check:
	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);
	rpmemd_db_pool_remove(rpmemd->db, req->pool_desc, 0, 0);
err_pool_create:
	free(rpmemd->pool_desc);
err_strdup:
err_pool_opened:
	if (err_send)
		ret = rpmemd_obc_create_resp(obc, status, &resp);
	rpmemd->closing = 1;
	return ret;
}

/*
 * rpmemd_req_open -- handle open request
 */
static int
rpmemd_req_open(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_req_attr *req)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "open request:");
	rpmemd_print_req_attr(req);
	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	int ret;
	int status = 0;
	int err_send = 1;
	struct rpmem_resp_attr resp;
	memset(&resp, 0, sizeof(resp));

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	if (rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool already opened");
		ret = -1;
		status = RPMEM_ERR_FATAL;
		goto err_pool_opened;
	}

	rpmemd->pool_desc = strdup(req->pool_desc);
	if (!rpmemd->pool_desc) {
		RPMEMD_LOG(ERR, "!allocating pool descriptor");
		ret = -1;
		status = RPMEM_ERR_FATAL;
		goto err_strdup;
	}

	rpmemd->pool = rpmemd_db_pool_open(rpmemd->db,
			req->pool_desc, 0, &pool_attr);
	if (!rpmemd->pool) {
		ret = -1;
		status = rpmemd_db_get_status(errno);
		goto err_pool_open;
	}

	RPMEMD_LOG(NOTICE, "pool attributes:");
	rpmemd_print_pool_attr(&pool_attr);

	ret = rpmemd_check_pool(rpmemd, req, &status);
	if (ret)
		goto err_pool_check;

	ret = rpmemd_common_fip_init(rpmemd, req, &resp, &status);
	if (ret)
		goto err_fip_init;

	RPMEMD_LOG(NOTICE, "open request response: (status = %u)", status);
	if (!status)
		rpmemd_print_resp_attr(&resp);

	ret = rpmemd_obc_open_resp(obc, status, &resp, &pool_attr);
	if (ret)
		goto err_open_resp;

	ret = rpmemd_fip_start_thread(rpmemd);
	if (ret)
		goto err_fip_start;

	return 0;
err_fip_start:
err_open_resp:
	err_send = 0;
	rpmemd_fip_fini(rpmemd->fip);
err_fip_init:
err_pool_check:
	rpmemd_db_pool_close(rpmemd->db, rpmemd->pool);
err_pool_open:
	free(rpmemd->pool_desc);
err_strdup:
err_pool_opened:
	if (err_send)
		ret = rpmemd_obc_open_resp(obc, status, &resp, &pool_attr);
	rpmemd->closing = 1;
	return ret;
}

/*
 * rpmemd_req_close -- handle close request
 */
static int
rpmemd_req_close(struct rpmemd_obc *obc, void *arg, int flags)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "close request");

	struct rpmemd *rpmemd = (struct rpmemd *)arg;

	rpmemd->closing = 1;

	int ret;
	int status = 0;

	if (!rpmemd->pool) {
		RPMEMD_LOG(ERR, "pool not opened");
		status = RPMEM_ERR_FATAL;
		return rpmemd_obc_close_resp(obc, status);
	}

	ret = rpmemd_fip_stop(rpmemd);
	if (ret) {
		status = RPMEM_ERR_FATAL;
	} else {
		rpmemd_fip_close(rpmemd->fip);
		rpmemd_fip_fini(rpmemd->fip);
	}

	int remove = rpmemd->created &&
			(status || (flags & RPMEM_CLOSE_FLAGS_REMOVE));
	if (rpmemd_close_pool(rpmemd, remove))
		RPMEMD_LOG(ERR, "closing pool failed");

	RPMEMD_LOG(NOTICE, "close request response (status = %u)", status);
	ret = rpmemd_obc_close_resp(obc, status);

	return ret;
}

/*
 * rpmemd_req_set_attr -- handle set attributes request
 */
static int
rpmemd_req_set_attr(struct rpmemd_obc *obc, void *arg,
	const struct rpmem_pool_attr *pool_attr)
{
	RPMEMD_ASSERT(arg != NULL);
	RPMEMD_LOG(NOTICE, "set attributes request");
	struct rpmemd *rpmemd = (struct rpmemd *)arg;
	RPMEMD_ASSERT(rpmemd->pool != NULL);

	int ret;
	int status = 0;
	int err_send = 1;

	ret = rpmemd_db_pool_set_attr(rpmemd->pool, pool_attr);
	if (ret) {
		ret = -1;
		status = rpmemd_db_get_status(errno);
		goto err_set_attr;
	}

	RPMEMD_LOG(NOTICE, "new pool attributes:");
	rpmemd_print_pool_attr(pool_attr);

	ret = rpmemd_obc_set_attr_resp(obc, status);
	if (ret)
		goto err_set_attr_resp;

	return ret;
err_set_attr_resp:
	err_send = 0;
err_set_attr:
	if (err_send)
		ret = rpmemd_obc_set_attr_resp(obc, status);
	return ret;
}

static struct rpmemd_obc_requests rpmemd_req = {
	.create		= rpmemd_req_create,
	.open		= rpmemd_req_open,
	.close		= rpmemd_req_close,
	.set_attr	= rpmemd_req_set_attr,
};

/*
 * rpmemd_print_info -- print basic info and configuration
 */
static void
rpmemd_print_info(struct rpmemd *rpmemd)
{
	RPMEMD_LOG(NOTICE, "ssh connection: %s",
			_str(os_getenv("SSH_CONNECTION")));
	RPMEMD_LOG(NOTICE, "user: %s", _str(os_getenv("USER")));
	RPMEMD_LOG(NOTICE, "configuration");
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "pool set directory: '%s'",
			_str(rpmemd->config.poolset_dir));
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "persist method: %s",
			rpmem_persist_method_to_str(rpmemd->persist_method));
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "number of threads: %lu",
			rpmemd->config.nthreads);
	RPMEMD_DBG(RPMEMD_LOG_INDENT "persist APM: %s",
			bool2str(rpmemd->config.persist_apm));
	RPMEMD_DBG(RPMEMD_LOG_INDENT "persist GPSPM: %s",
			bool2str(rpmemd->config.persist_general));
	RPMEMD_DBG(RPMEMD_LOG_INDENT "use syslog: %s",
			bool2str(rpmemd->config.use_syslog));
	RPMEMD_DBG(RPMEMD_LOG_INDENT "log file: %s",
			_str(rpmemd->config.log_file));
	RPMEMD_DBG(RPMEMD_LOG_INDENT "log level: %s",
			rpmemd_log_level_to_str(rpmemd->config.log_level));
}

int
main(int argc, char *argv[])
{
	util_init();

	int send_status = 1;
	int ret = 1;

	struct rpmemd *rpmemd = calloc(1, sizeof(*rpmemd));
	if (!rpmemd) {
		RPMEMD_LOG(ERR, "!calloc");
		goto err_rpmemd;
	}

	rpmemd->obc = rpmemd_obc_init(STDIN_FILENO, STDOUT_FILENO);
	if (!rpmemd->obc) {
		RPMEMD_LOG(ERR, "out-of-band connection initialization");
		goto err_obc;
	}

	if (rpmemd_log_init(DAEMON_NAME, NULL, 0)) {
		RPMEMD_LOG(ERR, "logging subsystem initialization failed");
		goto err_log_init;
	}

	if (rpmemd_config_read(&rpmemd->config, argc, argv) != 0) {
		RPMEMD_LOG(ERR, "reading configuration failed");
		goto err_config;
	}

	rpmemd_log_close();
	rpmemd_log_level = rpmemd->config.log_level;
	if (rpmemd_log_init(DAEMON_NAME, rpmemd->config.log_file,
				rpmemd->config.use_syslog)) {
		RPMEMD_LOG(ERR, "logging subsystem initialization"
			" failed (%s, %d)", rpmemd->config.log_file,
			rpmemd->config.use_syslog);
		goto err_log_init_config;
	}

	RPMEMD_LOG(INFO, "%s version %s", DAEMON_NAME, SRCVERSION);
	rpmemd->persist_method = rpmemd_get_pm(&rpmemd->config);

	rpmemd->db = rpmemd_db_init(rpmemd->config.poolset_dir, 0666);
	if (!rpmemd->db) {
		RPMEMD_LOG(ERR, "!pool set db initialization");
		goto err_db_init;
	}

	if (rpmemd->config.rm_poolset) {
		RPMEMD_LOG(INFO, "removing '%s'",
				rpmemd->config.rm_poolset);
		if (rpmemd_db_pool_remove(rpmemd->db,
				rpmemd->config.rm_poolset,
				rpmemd->config.force,
				rpmemd->config.pool_set)) {
			RPMEMD_LOG(ERR, "removing '%s' failed",
					rpmemd->config.rm_poolset);
			ret = errno;
		} else {
			RPMEMD_LOG(NOTICE, "removed '%s'",
					rpmemd->config.rm_poolset);
			ret = 0;
		}
		send_status = 0;
		goto out_rm;
	}

	ret = rpmemd_obc_status(rpmemd->obc, 0);
	if (ret) {
		RPMEMD_LOG(ERR, "writing status failed");
		goto err_status;
	}

	rpmemd_print_info(rpmemd);

	while (!ret) {
		ret = rpmemd_obc_process(rpmemd->obc, &rpmemd_req, rpmemd);
		if (ret) {
			RPMEMD_LOG(ERR, "out-of-band connection"
					" process failed");
			goto err;
		}

		if (rpmemd->closing)
			break;
	}

	rpmemd_db_fini(rpmemd->db);
	rpmemd_config_free(&rpmemd->config);
	rpmemd_log_close();
	rpmemd_obc_fini(rpmemd->obc);
	free(rpmemd);

	return 0;
err:
	rpmemd_req_cleanup(rpmemd);
err_status:
out_rm:
	rpmemd_db_fini(rpmemd->db);
err_db_init:
err_log_init_config:
	rpmemd_config_free(&rpmemd->config);
err_config:
	rpmemd_log_close();
err_log_init:
	if (send_status) {
		if (rpmemd_obc_status(rpmemd->obc, (uint32_t)errno))
			RPMEMD_LOG(ERR, "writing status failed");
	}
	rpmemd_obc_fini(rpmemd->obc);
err_obc:
	free(rpmemd);
err_rpmemd:
	return ret;
}
