/*
 * Copyright 2015-2016, Intel Corporation
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
 * ut_backtrace.c -- backtrace reporting routines
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "unittest.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

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

		if (dladdr(ptr, &dlinfo) && dlinfo.dli_fname &&
				*dlinfo.dli_fname)
			fname = dlinfo.dli_fname;

		UT_ERR("%u: %s (%s%s+0x%lx) [%p]", i++, fname, procname,
				ret == -UNW_ENOMEM ? "..." : "", off, ptr);

		ret = unw_step(&cursor);
		if (ret < 0)
			UT_ERR("unw_step: %s [%d]", unw_strerror(ret), ret);
	}
}
#else /* USE_LIBUNWIND */

#define SIZE 100

#ifndef _WIN32

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

#else /* _WIN32 */

#include <DbgHelp.h>

/*
 * ut_dump_backtrace -- dump stacktrace to error log
 */
void
ut_dump_backtrace(void)
{
	void *buffer[SIZE];
	unsigned nptrs;
	SYMBOL_INFO *symbol;

	HANDLE proc_hndl = GetCurrentProcess();
	SymInitialize(proc_hndl, NULL, TRUE);

	nptrs = CaptureStackBackTrace(0, SIZE, buffer, NULL);
	symbol = calloc(sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(CHAR), 1);
	symbol->MaxNameLen = MAX_SYM_NAME - 1;
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

	for (unsigned i = 0; i < nptrs; i++) {
		if (SymFromAddr(proc_hndl, (DWORD64)buffer[i], 0, symbol)) {
			UT_ERR("%u: %s [%p]", nptrs - i - 1, symbol->Name,
				buffer[i]);
		} else {
			UT_ERR("%u: [%p]", nptrs - i - 1, buffer[i]);
		}
	}

	free(symbol);
}

#endif /* _WIN32 */

#endif /* USE_LIBUNWIND */

/*
 * ut_sighandler -- fatal signal handler
 */
void
ut_sighandler(int sig)
{
	UT_ERR("\n");
	UT_ERR("Signal %d, backtrace:", sig);
	ut_dump_backtrace();
	UT_ERR("\n");
	exit(128 + sig);
}

/*
 * ut_register_sighandlers -- register signal handlers for various fatal signals
 */
void
ut_register_sighandlers()
{
	signal(SIGSEGV, ut_sighandler);
	signal(SIGABRT, ut_sighandler);
	signal(SIGILL, ut_sighandler);
	signal(SIGFPE, ut_sighandler);
	signal(SIGINT, ut_sighandler);
#ifndef _WIN32
	signal(SIGQUIT, ut_sighandler);
	signal(SIGBUS, ut_sighandler);
#endif
}
