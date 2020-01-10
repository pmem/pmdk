/*
 * Copyright 2014-2020, Intel Corporation
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
#include "os_thread.h"
#include "valgrind_internal.h"
#include "util.h"

/* XXX - modify Linux makefiles to generate srcversion.h and remove #ifdef */
#ifdef _WIN32
#include "srcversion.h"
#endif

static const char *Log_prefix;
static int Log_level;
static FILE *Out_fp;
static unsigned Log_alignment;

#ifndef NO_LIBPTHREAD
#define MAXPRINT 8192	/* maximum expected log line */
#else
#define MAXPRINT 256	/* maximum expected log line for libpmem */
#endif

struct errormsg
{
	char msg[MAXPRINT];
#ifdef _WIN32
	wchar_t wmsg[MAXPRINT];
#endif
};

#ifndef NO_LIBPTHREAD

static os_once_t Last_errormsg_key_once = OS_ONCE_INIT;
static os_tls_key_t Last_errormsg_key;

static void
_Last_errormsg_key_alloc(void)
{
	int pth_ret = os_tls_key_create(&Last_errormsg_key, free);
	if (pth_ret)
		FATAL("!os_thread_key_create");

	VALGRIND_ANNOTATE_HAPPENS_BEFORE(&Last_errormsg_key_once);
}

static void
Last_errormsg_key_alloc(void)
{
	os_once(&Last_errormsg_key_once, _Last_errormsg_key_alloc);
	/*
	 * Workaround Helgrind's bug:
	 * https://bugs.kde.org/show_bug.cgi?id=337735
	 */
	VALGRIND_ANNOTATE_HAPPENS_AFTER(&Last_errormsg_key_once);
}

static inline void
Last_errormsg_fini(void)
{
	void *p = os_tls_get(Last_errormsg_key);
	if (p) {
		free(p);
		(void) os_tls_set(Last_errormsg_key, NULL);
	}
	(void) os_tls_key_delete(Last_errormsg_key);
}

static inline struct errormsg *
Last_errormsg_get(void)
{
	Last_errormsg_key_alloc();

	struct errormsg *errormsg = os_tls_get(Last_errormsg_key);
	if (errormsg == NULL) {
		errormsg = malloc(sizeof(struct errormsg));
		if (errormsg == NULL)
			FATAL("!malloc");
		/* make sure it contains empty string initially */
		errormsg->msg[0] = '\0';
		int ret = os_tls_set(Last_errormsg_key, errormsg);
		if (ret)
			FATAL("!os_tls_set");
	}
	return errormsg;
}

#else

/*
 * We don't want libpmem to depend on libpthread.  Instead of using pthread
 * API to dynamically allocate thread-specific error message buffer, we put
 * it into TLS.  However, keeping a pretty large static buffer (8K) in TLS
 * may lead to some issues, so the maximum message length is reduced.
 * Fortunately, it looks like the longest error message in libpmem should
 * not be longer than about 90 chars (in case of pmem_check_version()).
 */

static __thread struct errormsg Last_errormsg;

static inline void
Last_errormsg_key_alloc(void)
{
}

static inline void
Last_errormsg_fini(void)
{
}

static inline const struct errormsg *
Last_errormsg_get(void)
{
	return &Last_errormsg.msg[0];
}

#endif /* NO_LIBPTHREAD */

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
			int ret = snprintf(log_file_pid, PATH_MAX, "%s%d",
				log_file, getpid());
			if (ret < 0 || ret >= PATH_MAX) {
				ERR("snprintf: %d", ret);
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
	LOG(1, "pid %d: program: %s", getpid(),
		util_getexecname(namepath, PATH_MAX));
#endif
	LOG(1, "%s version %d.%d", log_prefix, major_version, minor_version);

	static __attribute__((used)) const char *version_msg =
			"src version: " SRCVERSION;
	LOG(1, "%s", version_msg);
#if VG_PMEMCHECK_ENABLED
	/*
	 * Attribute "used" to prevent compiler from optimizing out the variable
	 * when LOG expands to no code (!DEBUG)
	 */
	static __attribute__((used)) const char *pmemcheck_msg =
			"compiled with support for Valgrind pmemcheck";
	LOG(1, "%s", pmemcheck_msg);
#endif /* VG_PMEMCHECK_ENABLED */
#if VG_HELGRIND_ENABLED
	static __attribute__((used)) const char *helgrind_msg =
			"compiled with support for Valgrind helgrind";
	LOG(1, "%s", helgrind_msg);
#endif /* VG_HELGRIND_ENABLED */
#if VG_MEMCHECK_ENABLED
	static __attribute__((used)) const char *memcheck_msg =
			"compiled with support for Valgrind memcheck";
	LOG(1, "%s", memcheck_msg);
#endif /* VG_MEMCHECK_ENABLED */
#if VG_DRD_ENABLED
	static __attribute__((used)) const char *drd_msg =
			"compiled with support for Valgrind drd";
	LOG(1, "%s", drd_msg);
#endif /* VG_DRD_ENABLED */
#if SDS_ENABLED
	static __attribute__((used)) const char *shutdown_state_msg =
			"compiled with support for shutdown state";
	LOG(1, "%s", shutdown_state_msg);
#endif
#if NDCTL_ENABLED
	static __attribute__((used)) const char *ndctl_ge_63_msg =
		"compiled with libndctl 63+";
	LOG(1, "%s", ndctl_ge_63_msg);
#endif

	Last_errormsg_key_alloc();
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

	Last_errormsg_fini();
}

/*
 * out_print_func -- default print_func, goes to stderr or Out_fp
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
 * calling Print(s) calls the current print_func...
 */
typedef void (*Print_func)(const char *s);
typedef int (*Vsnprintf_func)(char *str, size_t size, const char *format,
		va_list ap);
static Print_func Print = out_print_func;
static Vsnprintf_func Vsnprintf = vsnprintf;

/*
 * out_set_print_func -- allow override of print_func used by out module
 */
void
out_set_print_func(void (*print_func)(const char *s))
{
	LOG(3, "print %p", print_func);

	Print = (print_func == NULL) ? out_print_func : print_func;
}

/*
 * out_set_vsnprintf_func -- allow override of vsnprintf_func used by out module
 */
void
out_set_vsnprintf_func(int (*vsnprintf_func)(char *str, size_t size,
				const char *format, va_list ap))
{
	LOG(3, "vsnprintf %p", vsnprintf_func);

	Vsnprintf = (vsnprintf_func == NULL) ? vsnprintf : vsnprintf_func;
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
	ret = Vsnprintf(str, size, format, ap);
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

	unsigned long olast_error = 0;
#ifdef _WIN32
	if (fmt && fmt[0] == '!' && fmt[1] == '!')
		olast_error = GetLastError();
#endif

	if (file) {
		char *f = strrchr(file, OS_DIR_SEPARATOR);
		if (f)
			file = f + 1;
		ret = out_snprintf(&buf[cc], MAXPRINT - cc,
				"<%s>: <%d> [%s:%d %s] ",
				Log_prefix, level, file, line, func);
		if (ret < 0) {
			Print("out_snprintf failed");
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
			if (*fmt == '!') {
				fmt++;
				/* it will abort on non Windows OS */
				util_strwinerror(olast_error, errstr,
					UTIL_MAX_ERR_MSG);
			} else {
				util_strerror(oerrno, errstr, UTIL_MAX_ERR_MSG);
			}

		}
		ret = Vsnprintf(&buf[cc], MAXPRINT - cc, fmt, ap);
		if (ret < 0) {
			Print("Vsnprintf failed");
			goto end;
		}
		cc += (unsigned)ret;
	}

	out_snprintf(&buf[cc], MAXPRINT - cc, "%s%s%s", sep, errstr, suffix);

	Print(buf);

end:
	errno = oerrno;
#ifdef _WIN32
	SetLastError(olast_error);
#endif
}

/*
 * out_error -- common error output code, all error messages go through here
 */
static void
out_error(const char *file, int line, const char *func,
		const char *suffix, const char *fmt, va_list ap)
{
	int oerrno = errno;
	unsigned long olast_error = 0;
#ifdef _WIN32
	olast_error = GetLastError();
#endif
	unsigned cc = 0;
	int ret;
	const char *sep = "";
	char errstr[UTIL_MAX_ERR_MSG] = "";

	char *errormsg = (char *)out_get_errormsg();

	if (fmt) {
		if (*fmt == '!') {
			sep = ": ";
			fmt++;
			if (*fmt == '!') {
				fmt++;
				/* it will abort on non Windows OS */
				util_strwinerror(olast_error, errstr,
					UTIL_MAX_ERR_MSG);
			} else {
				util_strerror(oerrno, errstr, UTIL_MAX_ERR_MSG);
			}
		}

		ret = Vsnprintf(&errormsg[cc], MAXPRINT, fmt, ap);
		if (ret < 0) {
			strcpy(errormsg, "Vsnprintf failed");
			goto end;
		}
		cc += (unsigned)ret;
		out_snprintf(&errormsg[cc], MAXPRINT - cc, "%s%s",
				sep, errstr);
	}

#ifdef DEBUG
	if (Log_level >= 1) {
		char buf[MAXPRINT];
		cc = 0;

		if (file) {
			char *f = strrchr(file, OS_DIR_SEPARATOR);
			if (f)
				file = f + 1;
			ret = out_snprintf(&buf[cc], MAXPRINT,
					"<%s>: <1> [%s:%d %s] ",
					Log_prefix, file, line, func);
			if (ret < 0) {
				Print("out_snprintf failed");
				goto end;
			}
			cc += (unsigned)ret;
			if (cc < Log_alignment) {
				memset(buf + cc, ' ', Log_alignment - cc);
				cc = Log_alignment;
			}
		}

		out_snprintf(&buf[cc], MAXPRINT - cc, "%s%s", errormsg,
				suffix);

		Print(buf);
	}
#endif

end:
	errno = oerrno;
#ifdef _WIN32
	SetLastError(olast_error);
#endif
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
 * out_log -- output a log line if Log_level >= level
 */
void
out_log(const char *file, int line, const char *func, int level,
		const char *fmt, ...)
{
	va_list ap;

	if (Log_level < level)
		return;

	va_start(ap, fmt);
	out_common(file, line, func, level, "\n", fmt, ap);

	va_end(ap);
}

/*
 * out_fatal -- output a fatal error & die (i.e. assertion failure)
 */
void
out_fatal(const char *file, int line, const char *func,
		const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	out_common(file, line, func, 1, "\n", fmt, ap);

	va_end(ap);

	abort();
}

/*
 * out_err -- output an error message
 */
void
out_err(const char *file, int line, const char *func,
		const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	out_error(file, line, func, "\n", fmt, ap);

	va_end(ap);
}

/*
 * out_get_errormsg -- get the last error message
 */
const char *
out_get_errormsg(void)
{
	const struct errormsg *errormsg = Last_errormsg_get();
	return &errormsg->msg[0];
}

#ifdef _WIN32
/*
 * out_get_errormsgW -- get the last error message in wchar_t
 */
const wchar_t *
out_get_errormsgW(void)
{
	struct errormsg *errormsg = Last_errormsg_get();
	const char *utf8 = &errormsg->msg[0];
	wchar_t *utf16 = &errormsg->wmsg[0];
	if (util_toUTF16_buff(utf8, utf16, sizeof(errormsg->wmsg)) != 0)
		FATAL("!Failed to convert string");

	return (const wchar_t *)utf16;
}
#endif
