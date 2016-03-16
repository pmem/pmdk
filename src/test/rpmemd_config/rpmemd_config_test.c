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
 * rpmemd_log_test.c -- unit tests for rpmemd_log
 */

#include <stddef.h>
#include <inttypes.h>
#include <sys/param.h>
#include <syslog.h>

#include "unittest.h"
#include "rpmemd_log.h"
#include "rpmemd_config.h"

static const char *config_print_fmt =
"pid_file:\t\t%s\n"
"log_file\t\t%s\n"
"poolset_dir:\t\t%s\n"
"enable_remove:\t\t%s\n"
"enable_create:\t\t%s\n"
"foreground:\t\t%s\n"
"persist_apm:\t\t%s\n"
"persist_general:\t%s\n"
"provider_sockets:\t%s\n"
"provider_verbs:\t\t%s\n"
"use_syslog:\t\t%s\n"
"verify_pool_sets:\t%s\n"
"port:\t\t\t%hu\n"
"max_lanes:\t\t%" PRIu64 "\n"
"log_level:\t\t%s\n";

static inline const char *
bool_to_str(bool v)
{
	return v ? "yes" : "no";
}

static inline const char *
verify_pool_sets_to_str(struct rpmemd_config *config)
{
	return config->verify_pool_sets_auto ? "auto" :
		bool_to_str(config->verify_pool_sets);
}

static inline void
config_print(struct rpmemd_config *config)
{
	UT_ASSERT(config->log_level < MAX_RPD_LOG);

	printf(
		config_print_fmt,
		config->pid_file,
		config->log_file,
		config->poolset_dir,
		bool_to_str(config->enable_remove),
		bool_to_str(config->enable_create),
		bool_to_str(config->foreground),
		bool_to_str(config->persist_apm),
		bool_to_str(config->persist_general),
		bool_to_str(config->provider_sockets),
		bool_to_str(config->provider_verbs),
		bool_to_str(config->use_syslog),
		verify_pool_sets_to_str(config),
		config->port,
		config->max_lanes,
		rpmemd_log_level_to_str(config->log_level));
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_config");

	int ret = rpmemd_log_init("rpmemd_log", NULL, 0);
	UT_ASSERTeq(ret, 0);

	struct rpmemd_config config;

	UT_ASSERT(rpmemd_config_read(&config, argc, argv) == 0);

	config_print(&config);

	rpmemd_log_close();
	rpmemd_config_free(&config);

	DONE(NULL);
	return 0;
}
