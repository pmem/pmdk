// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * ut_backtrace.c -- backtrace reporting routines
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unittest.h"

#ifdef USE_LIBUNWIND

#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <dlfcn.h>

#define PROCNAMELEN 256
/*
 * ut_dump_backtrace -- dump stacktrace to error log using libunwind
 */
void
ut_dump_backtrace(void)
{
	unw_context_t context;
	unw_proc_info_t pip;

	pip.unwind_info = NULL;
	int ret = unw_getcontext(&context);
	if (ret) {
		UT_ERR("unw_getcontext: %s [%d]", unw_strerror(ret), ret);
		return;
	}

	unw_cursor_t cursor;
	ret = unw_init_local(&cursor, &context);
	if (ret) {
		UT_ERR("unw_init_local: %s [%d]", unw_strerror(ret), ret);
		return;
	}

	ret = unw_step(&cursor);

	char procname[PROCNAMELEN];
	unsigned i = 0;

	while (ret > 0) {
		ret = unw_get_proc_info(&cursor, &pip);
		if (ret) {
			UT_ERR("unw_get_proc_info: %s [%d]", unw_strerror(ret),
					ret);
			break;
		}

		unw_word_t off;
		ret = unw_get_proc_name(&cursor, procname, PROCNAMELEN, &off);
		if (ret && ret != -UNW_ENOMEM) {
			if (ret != -UNW_EUNSPEC) {
				UT_ERR("unw_get_proc_name: %s [%d]",
					unw_strerror(ret), ret);
			}

			strcpy(procname, "?");
		}

		void *ptr = (void *)(pip.start_ip + off);
		Dl_info dlinfo;
		const char *fname = "?";

		uintptr_t in_object_offset = 0;

		if (dladdr(ptr, &dlinfo) && dlinfo.dli_fname &&
				*dlinfo.dli_fname) {
			fname = dlinfo.dli_fname;

			uintptr_t base = (uintptr_t)dlinfo.dli_fbase;
			if ((uintptr_t)ptr >= base)
				in_object_offset = (uintptr_t)ptr - base;
		}

		UT_ERR("%u: %s (%s%s+0x%lx) [%p] [0x%" PRIxPTR "]",
			i++, fname, procname,
			ret == -UNW_ENOMEM ? "..." : "", off, ptr,
			in_object_offset);

		ret = unw_step(&cursor);
		if (ret < 0)
			UT_ERR("unw_step: %s [%d]", unw_strerror(ret), ret);
	}
}
#else /* USE_LIBUNWIND */

#define SIZE 100

#include <execinfo.h>

/*
 * ut_dump_backtrace -- dump stacktrace to error log using libc's backtrace
 */
void
ut_dump_backtrace(void)
{
	int j, nptrs;
	void *buffer[SIZE];
	char **strings;

	nptrs = backtrace(buffer, SIZE);

	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL) {
		UT_ERR("!backtrace_symbols");
		return;
	}

	for (j = 0; j < nptrs; j++)
		UT_ERR("%u: %s", j, strings[j]);

	free(strings);
}

#endif /* USE_LIBUNWIND */

/*
 * ut_sighandler -- fatal signal handler
 */
void
ut_sighandler(int sig)
{
	/*
	 * Usually SIGABRT is a result of ASSERT() or FATAL().
	 * We don't need backtrace, as the reason of the failure
	 * is logged in debug traces.
	 */
	if (sig != SIGABRT) {
		UT_ERR("\n");
		UT_ERR("Signal %d, backtrace:", sig);
		ut_dump_backtrace();
		UT_ERR("\n");
	}
	exit(128 + sig);
}

/*
 * ut_register_sighandlers -- register signal handlers for various fatal signals
 */
void
ut_register_sighandlers(void)
{
	signal(SIGSEGV, ut_sighandler);
	signal(SIGABRT, ut_sighandler);
	signal(SIGILL, ut_sighandler);
	signal(SIGFPE, ut_sighandler);
	signal(SIGINT, ut_sighandler);
	signal(SIGQUIT, ut_sighandler);
	signal(SIGBUS, ut_sighandler);
}
