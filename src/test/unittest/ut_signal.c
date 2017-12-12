/*
 * Copyright 2014-2017, Intel Corporation
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

/*
 * Sigactions[] - allows registering more than one signal/exception handler
 */
static struct sigaction Sigactions[NSIG];

/*
 * exception_handler -- called for unhandled exceptions
 */
static LONG CALLBACK
exception_handler(_In_ PEXCEPTION_POINTERS ExceptionInfo)
{
	DWORD excode = ExceptionInfo->ExceptionRecord->ExceptionCode;
	if (excode == EXCEPTION_ACCESS_VIOLATION)
		Sigactions[SIGSEGV].sa_handler(SIGSEGV);
	return EXCEPTION_CONTINUE_EXECUTION;
}

/*
 * signal_handler_wrapper -- (internal) wrapper for user-defined signal handler
 *
 * Before the specified handler function is executed, signal disposition
 * is reset to SIG_DFL.  This wrapper allows to handle subsequent signals
 * without the need to set the signal disposition again.
 */
static void
signal_handler_wrapper(int signum)
{
	_crt_signal_t retval = signal(signum, signal_handler_wrapper);
	if (retval == SIG_ERR)
		UT_FATAL("!signal: %d", signum);

	if (Sigactions[signum].sa_handler)
		Sigactions[signum].sa_handler(signum);
	else
		UT_FATAL("handler for signal: %d is not defined", signum);
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
		ut_fatal(file, line, func, "!sigaction: %s",
			os_strsignal(signum));
	return retval;
#else
	UT_ASSERT(signum < NSIG);
	os_mutex_lock(&Sigactions_lock);
	if (oldact)
		*oldact = Sigactions[signum];
	if (act)
		Sigactions[signum] = *act;
	os_mutex_unlock(&Sigactions_lock);

	if (signum == SIGABRT) {
		ut_suppress_errmsg();
	}
	if (signum == SIGSEGV) {
		AddVectoredExceptionHandler(0, exception_handler);
	}

	_crt_signal_t retval = signal(signum, signal_handler_wrapper);
	if (retval == SIG_ERR)
		ut_fatal(file, line, func, "!signal: %d", signum);

	if (oldact != NULL)
		oldact->sa_handler = retval;

	return 0;
#endif
}
