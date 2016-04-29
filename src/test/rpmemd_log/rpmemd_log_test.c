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
#include <sys/param.h>
#include <syslog.h>

#include "unittest.h"
#include "rpmemd_log.h"

#define PREFIX	"prefix"

FILE *syslog_fh;

/*
 * openlog -- mock for openlog function which logs its usage
 */
FUNC_MOCK(openlog, void, const char *ident, int option, int facility)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("openlog: ident = %s, option = %d, facility = %d",
			ident, option, facility);
}
FUNC_MOCK_END

/*
 * closelog -- mock for closelog function which logs its usage
 */
FUNC_MOCK(closelog, void, void)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("closelog");
}
FUNC_MOCK_END

/*
 * syslog -- mock for syslog function which redirects message to a file
 */
FUNC_MOCK(syslog, void, int priority, const char *format, ...)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERT(priority == LOG_ERR ||
		priority == LOG_WARNING ||
		priority == LOG_NOTICE ||
		priority == LOG_INFO ||
		priority == LOG_DEBUG);

	va_list ap;
	va_start(ap, format);
	vfprintf(syslog_fh, format, ap);
	va_end(ap);
}
FUNC_MOCK_END

/*
 * vsyslog -- mock for vsyslog function which redirects message to a file
 */
FUNC_MOCK(vsyslog, void, int priority, const char *format, va_list ap)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERT(priority == LOG_ERR ||
		priority == LOG_WARNING ||
		priority == LOG_NOTICE ||
		priority == LOG_INFO ||
		priority == LOG_DEBUG);

	vfprintf(syslog_fh, format, ap);
}
FUNC_MOCK_END

/*
 * l2s -- level to string
 */
static const char *
l2s(enum rpmemd_log_level level)
{
	return rpmemd_log_level_to_str(level);
}

/*
 * test_log_messages -- test log messages on specified level
 */
static void
test_log_messages(enum rpmemd_log_level level)
{
	rpmemd_log_level = level;

	RPMEMD_LOG(ERR, "ERR message on %s level", l2s(level));
	RPMEMD_LOG(WARN, "WARN message on %s level", l2s(level));
	RPMEMD_LOG(NOTICE, "NOTICE message on %s level", l2s(level));
	RPMEMD_LOG(INFO, "INFO message on %s level", l2s(level));
	RPMEMD_DBG("DBG message on %s level", l2s(level));
}

/*
 * test_all_log_messages -- test log messages on all levels, with and without
 * a prefix.
 */
static void
test_all_log_messages(void)
{
	rpmemd_prefix(NULL);
	test_log_messages(RPD_LOG_ERR);
	test_log_messages(RPD_LOG_WARN);
	test_log_messages(RPD_LOG_NOTICE);
	test_log_messages(RPD_LOG_INFO);
	test_log_messages(_RPD_LOG_DBG);

	rpmemd_prefix("[%s]", PREFIX);
	test_log_messages(RPD_LOG_ERR);
	test_log_messages(RPD_LOG_WARN);
	test_log_messages(RPD_LOG_NOTICE);
	test_log_messages(RPD_LOG_INFO);
	test_log_messages(_RPD_LOG_DBG);
}

#define USAGE() do {\
	UT_ERR("usage: %s fatal|log|assert "\
		"stdout|file|syslog <file>", argv[0]);\
} while (0)

enum test_log_type {
	TEST_STDOUT,
	TEST_FILE,
	TEST_SYSLOG,
};

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_log");

	if (argc < 4) {
		USAGE();
		return 1;
	}

	const char *log_op = argv[1];
	const char *log_type = argv[2];
	const char *file = argv[3];

	int do_fatal = 0;
	int do_assert = 0;
	if (strcmp(log_op, "fatal") == 0) {
		do_fatal = 1;
	} else if (strcmp(log_op, "assert") == 0) {
		do_assert = 1;
	} else if (strcmp(log_op, "log") == 0) {
	} else {
		USAGE();
		return 1;
	}

	enum test_log_type type;
	if (strcmp(log_type, "stdout") == 0) {
		type = TEST_STDOUT;
	} else if (strcmp(log_type, "file") == 0) {
		type = TEST_FILE;
	} else if (strcmp(log_type, "syslog") == 0) {
		type = TEST_SYSLOG;
	} else {
		USAGE();
		return 1;
	}

	int fd_stdout = -1;
	FILE *stdout_fh = NULL;
	switch (type) {
	case TEST_STDOUT:
		/*
		 * Duplicate stdout file descriptor in order to preserve
		 * the file list after redirecting the stdout to a file.
		 */
		fd_stdout = dup(1);
		UT_ASSERTne(fd_stdout, -1);
		close(1);
		stdout_fh = fopen(file, "a");
		UT_ASSERTne(stdout_fh, NULL);
		break;
	case TEST_SYSLOG:
		syslog_fh = fopen(file, "a");
		UT_ASSERTne(syslog_fh, NULL);
		break;
	default:
		break;
	}

	/*
	 * Check an invalid configuration
	 */
	int ret;
	ret = rpmemd_log_init("rpmemd_log", file, 1);
	UT_ASSERTne(ret, 0);

	switch (type) {
	case TEST_STDOUT:
		ret = rpmemd_log_init("rpmemd_log", NULL, 0);
		UT_ASSERTeq(ret, 0);
		break;
	case TEST_SYSLOG:
		ret = rpmemd_log_init("rpmemd_log", NULL, 1);
		UT_ASSERTeq(ret, 0);
		break;
	case TEST_FILE:
		ret = rpmemd_log_init("rpmemd_log", file, 0);
		UT_ASSERTeq(ret, 0);
		break;
	default:
		break;
	}

	if (do_fatal) {
		RPMEMD_FATAL("fatal");
	} else if (do_assert) {
		RPMEMD_ASSERT(1);
		RPMEMD_ASSERT(0);
	} else {
		test_all_log_messages();
	}

	rpmemd_log_close();

	switch (type) {
	case TEST_STDOUT:
		/* restore the original stdout file descriptor */
		fclose(stdout_fh);
		UT_ASSERTeq(dup2(fd_stdout, 1), 1);
		close(fd_stdout);
		break;
	case TEST_SYSLOG:
		fclose(syslog_fh);
		break;
	default:
		break;
	}

	DONE(NULL);
}
