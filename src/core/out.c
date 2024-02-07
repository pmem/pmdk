// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * out.c -- support for logging, tracing, and assertion output
 *
 * Macros like LOG(), OUT, ASSERT(), etc. end up here.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "out.h"
#include "os.h"
#include "valgrind_internal.h"
#include "util.h"
#include "log_internal.h"
#include "last_error_msg.h"

#define MAXPRINT CORE_LAST_ERROR_MSG_MAXPRINT

static const char *Log_prefix;
static int Log_level;
static FILE *Out_fp;
static unsigned Log_alignment;

/*
 * out_init -- initialize the log
 *
 * This is called from the library initialization code.
 */
void
out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var, int major_version,
		int minor_version)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(log_level_var, log_file_var);

	static int once;

	/* only need to initialize the out module once */
	if (once)
		return;
	once++;

	Log_prefix = log_prefix;

#ifdef DEBUG
	char *log_level;
	char *log_file;

	if ((log_level = os_getenv(log_level_var)) != NULL) {
		Log_level = atoi(log_level);
		if (Log_level < 0) {
			Log_level = 0;
		}
	}

	if ((log_file = os_getenv(log_file_var)) != NULL &&
				log_file[0] != '\0') {

		/* reserve more than enough space for a PID + '\0' */
		char log_file_pid[PATH_MAX];
		size_t len = strlen(log_file);
		if (len > 0 && log_file[len - 1] == '-') {
			if (util_snprintf(log_file_pid, PATH_MAX, "%s%d",
					log_file, getpid()) < 0) {
				ERR_W_ERRNO("snprintf");
				abort();
			}
			log_file = log_file_pid;
		}

		if ((Out_fp = os_fopen(log_file, "w")) == NULL) {
			char buff[UTIL_MAX_ERR_MSG];
			util_strerror(errno, buff, UTIL_MAX_ERR_MSG);
			fprintf(stderr, "Error (%s): %s=%s: %s\n",
				log_prefix, log_file_var,
				log_file, buff);
			abort();
		}
	}
#endif	/* DEBUG */

	char *log_alignment = os_getenv("PMDK_LOG_ALIGN");
	if (log_alignment) {
		int align = atoi(log_alignment);
		if (align > 0)
			Log_alignment = (unsigned)align;
	}

	if (Out_fp == NULL)
		Out_fp = stderr;
	else
		setlinebuf(Out_fp);

#ifdef DEBUG
	static char namepath[PATH_MAX];
	CORE_LOG_ALWAYS("pid %d: program: %s", getpid(),
		util_getexecname(namepath, PATH_MAX));
#endif
	CORE_LOG_ALWAYS("%s version %d.%d", log_prefix, major_version,
		minor_version);

	static __attribute__((used)) const char *version_msg =
			"src version: " SRCVERSION;
	CORE_LOG_ALWAYS("%s", version_msg);
#if VG_PMEMCHECK_ENABLED
	/*
	 * Attribute "used" to prevent compiler from optimizing out the variable
	 * when LOG expands to no code (!DEBUG)
	 */
	static __attribute__((used)) const char *pmemcheck_msg =
			"compiled with support for Valgrind pmemcheck";
	CORE_LOG_ALWAYS("%s", pmemcheck_msg);
#endif /* VG_PMEMCHECK_ENABLED */
#if VG_HELGRIND_ENABLED
	static __attribute__((used)) const char *helgrind_msg =
			"compiled with support for Valgrind helgrind";
	CORE_LOG_ALWAYS("%s", helgrind_msg);
#endif /* VG_HELGRIND_ENABLED */
#if VG_MEMCHECK_ENABLED
	static __attribute__((used)) const char *memcheck_msg =
			"compiled with support for Valgrind memcheck";
	CORE_LOG_ALWAYS("%s", memcheck_msg);
#endif /* VG_MEMCHECK_ENABLED */
#if VG_DRD_ENABLED
	static __attribute__((used)) const char *drd_msg =
			"compiled with support for Valgrind drd";
	CORE_LOG_ALWAYS("%s", drd_msg);
#endif /* VG_DRD_ENABLED */
#if SDS_ENABLED
	static __attribute__((used)) const char *shutdown_state_msg =
			"compiled with support for shutdown state";
	CORE_LOG_ALWAYS("%s", shutdown_state_msg);
#endif
#if NDCTL_ENABLED
	static __attribute__((used)) const char *ndctl_ge_63_msg =
		"compiled with libndctl 63+";
	CORE_LOG_ALWAYS("%s", ndctl_ge_63_msg);
#endif

	last_error_msg_init();
}

/*
 * out_fini -- close the log file
 *
 * This is called to close log file before process stop.
 */
void
out_fini(void)
{
	if (Out_fp != NULL && Out_fp != stderr) {
		fclose(Out_fp);
		Out_fp = stderr;
	}
}

/*
 * out_print_func -- print function, goes to Out_fp (stderr by default)
 */
static void
out_print_func(const char *s)
{
	/* to suppress drd false-positive */
	/* XXX: confirm real nature of this issue: pmem/issues#863 */
#ifdef SUPPRESS_FPUTS_DRD_ERROR
	VALGRIND_ANNOTATE_IGNORE_READS_BEGIN();
	VALGRIND_ANNOTATE_IGNORE_WRITES_BEGIN();
#endif
	fputs(s, Out_fp);
#ifdef SUPPRESS_FPUTS_DRD_ERROR
	VALGRIND_ANNOTATE_IGNORE_READS_END();
	VALGRIND_ANNOTATE_IGNORE_WRITES_END();
#endif
}

/*
 * out_snprintf -- (internal) custom snprintf implementation
 */
FORMAT_PRINTF(3, 4)
static int
out_snprintf(char *str, size_t size, const char *format, ...)
{
	int ret;
	va_list ap;

	va_start(ap, format);
	ret = vsnprintf(str, size, format, ap);
	va_end(ap);

	return (ret);
}

/*
 * out_common -- common output code, all output goes through here
 */
static void
out_common(const char *file, int line, const char *func, int level,
		const char *suffix, const char *fmt, va_list ap)
{
	int oerrno = errno;
	char buf[MAXPRINT];
	unsigned cc = 0;
	int ret;
	const char *sep = "";
	char errstr[UTIL_MAX_ERR_MSG] = "";

	if (file) {
		char *f = strrchr(file, OS_DIR_SEPARATOR);
		if (f)
			file = f + 1;
		ret = out_snprintf(&buf[cc], MAXPRINT - cc,
				"<%s>: <%d> [%s:%d %s] ",
				Log_prefix, level, file, line, func);
		if (ret < 0) {
			out_print_func("out_snprintf failed");
			goto end;
		}
		cc += (unsigned)ret;
		if (cc < Log_alignment) {
			memset(buf + cc, ' ', Log_alignment - cc);
			cc = Log_alignment;
		}
	}

	if (fmt) {
		if (*fmt == '!') {
			sep = ": ";
			fmt++;
			util_strerror(oerrno, errstr, UTIL_MAX_ERR_MSG);
		}
		ret = vsnprintf(&buf[cc], MAXPRINT - cc, fmt, ap);
		if (ret < 0) {
			out_print_func("vsnprintf failed");
			goto end;
		}
		cc += (unsigned)ret;
	}

	out_snprintf(&buf[cc], MAXPRINT - cc, "%s%s%s", sep, errstr, suffix);

	out_print_func(buf);

end:
	errno = oerrno;
}

/*
 * out -- output a line, newline added automatically
 */
void
out(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	out_common(NULL, 0, NULL, 0, "\n", fmt, ap);

	va_end(ap);
}

/*
 * out_nonl -- output a line, no newline added automatically
 */
void
out_nonl(int level, const char *fmt, ...)
{
	va_list ap;

	if (Log_level < level)
			return;

	va_start(ap, fmt);
	out_common(NULL, 0, NULL, level, "", fmt, ap);

	va_end(ap);
}

/*
 * out_log_va/out_log -- output a log line if Log_level >= level
 */
void
out_log_va(const char *file, int line, const char *func, int level,
		const char *fmt, va_list ap)
{
	if (Log_level < level)
		return;
	out_common(file, line, func, level, "\n", fmt, ap);
}

void
out_log(const char *file, int line, const char *func, int level,
		const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	out_log_va(file, line, func, level, fmt, ap);

	va_end(ap);
}
