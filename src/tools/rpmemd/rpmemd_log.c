/*
 * Copyright 2016-2017, Intel Corporation
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
 * rpmemd_log.c -- rpmemd logging functions definitions
 */
/* for GNU version of basename */
#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

#include "rpmemd_log.h"

#define RPMEMD_SYSLOG_OPTS	(LOG_NDELAY | LOG_PID)
#define RPMEMD_SYSLOG_FACILITY	(LOG_USER)
#define RPMEMD_DEFAULT_FH	stderr
#define RPMEMD_MAX_MSG		((size_t)8192)
#define RPMEMD_MAX_PREFIX	((size_t)256)

enum rpmemd_log_level rpmemd_log_level;
static char *rpmemd_ident;
static int rpmemd_use_syslog;
static FILE *rpmemd_log_file;
static char rpmemd_prefix_buff[RPMEMD_MAX_PREFIX];

static const char *rpmemd_log_level_str[MAX_RPD_LOG] = {
	[RPD_LOG_ERR]		= "err",
	[RPD_LOG_WARN]		= "warn",
	[RPD_LOG_NOTICE]	= "notice",
	[RPD_LOG_INFO]		= "info",
	[_RPD_LOG_DBG]		= "debug",
};

static int rpmemd_level2prio[MAX_RPD_LOG] = {
	[RPD_LOG_ERR]		= LOG_ERR,
	[RPD_LOG_WARN]		= LOG_WARNING,
	[RPD_LOG_NOTICE]	= LOG_NOTICE,
	[RPD_LOG_INFO]		= LOG_INFO,
	[_RPD_LOG_DBG]		= LOG_DEBUG,
};

/*
 * rpmemd_log_level_from_str -- converts string to log level value
 */
enum rpmemd_log_level
rpmemd_log_level_from_str(const char *str)
{
	if (!str)
		return MAX_RPD_LOG;

	for (enum rpmemd_log_level level = 0; level < MAX_RPD_LOG; level++) {
		if (strcmp(rpmemd_log_level_str[level], str) == 0)
			return level;
	}

	return MAX_RPD_LOG;
}

/*
 * rpmemd_log_level_to_str -- converts log level enum to string
 */
const char *
rpmemd_log_level_to_str(enum rpmemd_log_level level)
{
	if (level >= MAX_RPD_LOG)
		return NULL;
	return rpmemd_log_level_str[level];
}

/*
 * rpmemd_log_init -- inititalize logging subsystem
 *
 * ident      - string prepended to every message
 * use_syslog - use syslog instead of standard output
 */
int
rpmemd_log_init(const char *ident, const char *fname, int use_syslog)
{
	rpmemd_use_syslog = use_syslog;

	if (rpmemd_use_syslog) {
		openlog(rpmemd_ident, RPMEMD_SYSLOG_OPTS,
				RPMEMD_SYSLOG_FACILITY);
	} else {
		rpmemd_ident = strdup(ident);
		if (!rpmemd_ident) {
			perror("strdup");
			return -1;
		}

		if (fname) {
			rpmemd_log_file = fopen(fname, "a");
			if (!rpmemd_log_file) {
				perror(fname);
				return -1;
			}
		} else {
			rpmemd_log_file = RPMEMD_DEFAULT_FH;
		}
	}

	return 0;
}

/*
 * rpmemd_log_close -- deinitialize logging subsystem
 */
void
rpmemd_log_close(void)
{
	if (rpmemd_use_syslog) {
		closelog();
	} else {
		if (rpmemd_log_file != RPMEMD_DEFAULT_FH)
			fclose(rpmemd_log_file);

		free(rpmemd_ident);
	}
}

/*
 * rpmemd_prefix -- set prefix for every message
 */
int
rpmemd_prefix(const char *fmt, ...)
{
	if (!fmt) {
		rpmemd_prefix_buff[0] = '\0';
		return 0;
	}

	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(rpmemd_prefix_buff, RPMEMD_MAX_PREFIX,
			fmt, ap);
	va_end(ap);
	if (ret < 0)
		return -1;

	return 0;
}

/*
 * rpmemd_log -- main logging function
 */
void
rpmemd_log(enum rpmemd_log_level level, const char *fname, int lineno,
	const char *fmt, ...)
{
	if (!rpmemd_use_syslog && level > rpmemd_log_level)
		return;

	char buff[RPMEMD_MAX_MSG];

	size_t cnt = 0;
	int ret;
	if (fname) {
		ret = snprintf(&buff[cnt], RPMEMD_MAX_MSG - cnt,
				"[%s:%d] ", basename(fname), lineno);
		if (ret < 0)
			RPMEMD_FATAL("snprintf failed");

		cnt += (size_t)ret;
	}
	if (rpmemd_prefix_buff[0]) {
		ret = snprintf(&buff[cnt], RPMEMD_MAX_MSG - cnt,
				"%s ", rpmemd_prefix_buff);
		if (ret < 0)
			RPMEMD_FATAL("snprintf failed");

		cnt += (size_t)ret;
	}

	const char *errorstr = "";
	const char *prefix = "";
	const char *suffix = "\n";
	if (fmt) {
		if (*fmt == '!') {
			fmt++;
			errorstr = strerror(errno);
			prefix = ": ";
		}
	}

	va_list ap;
	va_start(ap, fmt);
	ret = vsnprintf(&buff[cnt], RPMEMD_MAX_MSG - cnt, fmt, ap);
	va_end(ap);

	if (ret < 0)
		RPMEMD_FATAL("vsnprintf failed");

	cnt += (size_t)ret;

	ret = snprintf(&buff[cnt], RPMEMD_MAX_MSG - cnt,
			"%s%s%s", prefix, errorstr, suffix);
	if (ret < 0)
		RPMEMD_FATAL("snprintf failed");

	if (rpmemd_use_syslog) {
		int prio = rpmemd_level2prio[level];
		syslog(prio, "%s", buff);
	} else {
		fprintf(rpmemd_log_file, "%s", buff);
		fflush(rpmemd_log_file);
	}

}
