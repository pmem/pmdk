// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmemd_config.h -- internal definitions for rpmemd config
 */

#include <stdint.h>
#include <stdbool.h>

#ifndef RPMEMD_DEFAULT_LOG_FILE
#define RPMEMD_DEFAULT_LOG_FILE ("/var/log/" DAEMON_NAME ".log")
#endif

#ifndef RPMEMD_GLOBAL_CONFIG_FILE
#define RPMEMD_GLOBAL_CONFIG_FILE ("/etc/" DAEMON_NAME "/" DAEMON_NAME\
	".conf")
#endif

#define RPMEMD_USER_CONFIG_FILE ("." DAEMON_NAME ".conf")

#define RPMEM_DEFAULT_MAX_LANES	1024

#define RPMEM_DEFAULT_NTHREADS 0

#define HOME_ENV "HOME"

#define HOME_STR_PLACEHOLDER ("$" HOME_ENV)

struct rpmemd_config {
	char *log_file;
	char *poolset_dir;
	const char *rm_poolset;
	bool force;
	bool pool_set;
	bool persist_apm;
	bool persist_general;
	bool use_syslog;
	uint64_t max_lanes;
	enum rpmemd_log_level log_level;
	size_t nthreads;
};

int rpmemd_config_read(struct rpmemd_config *config, int argc, char *argv[]);
void rpmemd_config_free(struct rpmemd_config *config);
