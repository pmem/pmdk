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
