/*
 * Copyright 2014-2016, Intel Corporation
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
 * ut_signal.c -- unit test signal operations
 */

#include "unittest.h"

#ifdef _WIN32
/*
 * On Windows, Access Violation exception does not raise SIGSEGV signal.
 * The trick is to catch the exception and... call the signal handler.
 */

struct signal_handler {
	int signum;
	void(*Sa_handler)(int);
};

static int defined_handlers = 0;

/*
 * find_handler_index -- retreive signals array to find assigned handler
 */
int
find_handler_index(int signum)
{
	int used_handler = -1;

	for (int i = 0; i < defined_handlers; i++) {
		if (Sa_handler_tab[i].signum == signum) {
			used_handler = i;
			break;
		}
	}
	return used_handler;
}

/*
 * exception_handler -- called for unhandled exceptions
 */
static LONG CALLBACK
exception_handler(_In_ PEXCEPTION_POINTERS ExceptionInfo)
{
	DWORD excode = ExceptionInfo->ExceptionRecord->ExceptionCode;
	if (excode == EXCEPTION_ACCESS_VIOLATION) {
		int index = find_handler_index(SIGSEGV);
		if (index == -1) {
			UT_FATAL("!signal: %d is not defined in handler array",
				SIGSEGV);
		} else {
			Sa_handler_tab[index].Sa_handler(SIGSEGV);
		}
	}
	return EXCEPTION_CONTINUE_EXECUTION;
}

/*
 * exception_handler_sig_wrapper - called for set handler default func,
 * by default after execution exception handler func value is set to
 * SIG_DFL, this enables handle more than one exceptions
 * without new signal initialization
 */
static void
exception_handler_sig_wrapper(int signum)
{
	_crt_signal_t retval = signal(signum, exception_handler_sig_wrapper);
	if (retval == SIG_ERR)
		UT_FATAL("!signal: %d", signum);

	int index = find_handler_index(signum);
	if (index == -1)
		UT_FATAL("!signal: %d is not defined in handler array", signum);
	else
		Sa_handler_tab[index].Sa_handler(signum);
}
#endif

/*
 * ut_sigaction -- a sigaction that cannot return < 0
 */
int
ut_sigaction(const char *file, int line, const char *func,
		int signum, struct sigaction *act, struct sigaction *oldact)
{
#ifndef _WIN32
	int retval = sigaction(signum, act, oldact);
	if (retval != 0)
		ut_fatal(file, line, func, "!sigaction: %s", strsignal(signum));
	return retval;
#else
	int handler_assigned = 0;

	if (Sa_handler_tab == NULL) {
		Sa_handler_tab = (struct signal_handler *)
			MALLOC(sizeof(struct signal_handler));
		Sa_handler_tab->signum = signum;
		Sa_handler_tab->Sa_handler = act->sa_handler;
		defined_handlers++;
		handler_assigned = 1;
	}

	for (int i = 0; i < defined_handlers && handler_assigned != 1; i++) {
		if (Sa_handler_tab[i].signum == signum) {
			Sa_handler_tab[i].Sa_handler = act->sa_handler;
			handler_assigned = 1;
			break;
		}
	}

	if (handler_assigned == 0) {
		Sa_handler_tab = (struct signal_handler *)
			REALLOC(Sa_handler_tab,
			sizeof(struct signal_handler) * (defined_handlers + 1));
		Sa_handler_tab[defined_handlers].signum = signum;
		Sa_handler_tab[defined_handlers].Sa_handler =
			act->sa_handler;
		defined_handlers++;
	}

	if (signum == SIGABRT) {
		ut_suppress_errmsg();
	}
	if (signum == SIGSEGV) {
		AddVectoredExceptionHandler(0, exception_handler);
	}

	_crt_signal_t retval = signal(signum, exception_handler_sig_wrapper);
	if (retval == SIG_ERR)
		ut_fatal(file, line, func, "!signal: %d", signum);

	if (oldact != NULL) {
		oldact->sa_handler = retval;
	}
	return 0;
#endif
}
