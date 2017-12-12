/*
 * Copyright 2017, Intel Corporation
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
 * signals_fbsd.h - Signal definitions for FreeBSD
 */
#ifndef _SIGNALS_FBSD_H
#define _SIGNALS_FBSD_H 1

#define SIGNAL_2_STR(sig) [sig] = #sig

static const char *signal2str[] = {
	SIGNAL_2_STR(SIGHUP),	/*  1 */
	SIGNAL_2_STR(SIGINT),	/*  2 */
	SIGNAL_2_STR(SIGQUIT),	/*  3 */
	SIGNAL_2_STR(SIGILL),	/*  4 */
	SIGNAL_2_STR(SIGTRAP),	/*  5 */
	SIGNAL_2_STR(SIGABRT),	/*  6 */
	SIGNAL_2_STR(SIGEMT),	/*  7 */
	SIGNAL_2_STR(SIGFPE),	/*  8 */
	SIGNAL_2_STR(SIGKILL),	/*  9 */
	SIGNAL_2_STR(SIGBUS),	/* 10 */
	SIGNAL_2_STR(SIGSEGV),	/* 11 */
	SIGNAL_2_STR(SIGSYS),	/* 12 */
	SIGNAL_2_STR(SIGPIPE),	/* 13 */
	SIGNAL_2_STR(SIGALRM),	/* 14 */
	SIGNAL_2_STR(SIGTERM),	/* 15 */
	SIGNAL_2_STR(SIGURG),	/* 16 */
	SIGNAL_2_STR(SIGSTOP),	/* 17 */
	SIGNAL_2_STR(SIGTSTP),	/* 18 */
	SIGNAL_2_STR(SIGCONT),	/* 19 */
	SIGNAL_2_STR(SIGCHLD),	/* 20 */
	SIGNAL_2_STR(SIGTTIN),	/* 21 */
	SIGNAL_2_STR(SIGTTOU),	/* 22 */
	SIGNAL_2_STR(SIGIO),	/* 23 */
	SIGNAL_2_STR(SIGXCPU),	/* 24 */
	SIGNAL_2_STR(SIGXFSZ),	/* 25 */
	SIGNAL_2_STR(SIGVTALRM), /* 26 */
	SIGNAL_2_STR(SIGPROF),	/* 27 */
	SIGNAL_2_STR(SIGWINCH),	/* 28 */
	SIGNAL_2_STR(SIGINFO),	/* 29 */
	SIGNAL_2_STR(SIGUSR1),	/* 30 */
	SIGNAL_2_STR(SIGUSR2),	/* 31 */
	SIGNAL_2_STR(SIGTHR),	/* 32 */
	SIGNAL_2_STR(SIGLIBRT)	/* 33 */
};
#define SIGNALMAX SIGLIBRT

#endif
