// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * signals_linux.h - Signal definitions for Linux
 */
#ifndef _SIGNALS_LINUX_H
#define _SIGNALS_LINUX_H 1

#define SIGNAL_2_STR(sig) [sig] = #sig
static const char *signal2str[] = {
	SIGNAL_2_STR(SIGHUP),	/*  1 */
	SIGNAL_2_STR(SIGINT),	/*  2 */
	SIGNAL_2_STR(SIGQUIT),	/*  3 */
	SIGNAL_2_STR(SIGILL),	/*  4 */
	SIGNAL_2_STR(SIGTRAP),	/*  5 */
	SIGNAL_2_STR(SIGABRT),	/*  6 */
	SIGNAL_2_STR(SIGBUS),	/*  7 */
	SIGNAL_2_STR(SIGFPE),	/*  8 */
	SIGNAL_2_STR(SIGKILL),	/*  9 */
	SIGNAL_2_STR(SIGUSR1),	/* 10 */
	SIGNAL_2_STR(SIGSEGV),	/* 11 */
	SIGNAL_2_STR(SIGUSR2),	/* 12 */
	SIGNAL_2_STR(SIGPIPE),	/* 13 */
	SIGNAL_2_STR(SIGALRM),	/* 14 */
	SIGNAL_2_STR(SIGTERM),	/* 15 */
	SIGNAL_2_STR(SIGSTKFLT), /* 16 */
	SIGNAL_2_STR(SIGCHLD),	/* 17 */
	SIGNAL_2_STR(SIGCONT),	/* 18 */
	SIGNAL_2_STR(SIGSTOP),	/* 19 */
	SIGNAL_2_STR(SIGTSTP),	/* 20 */
	SIGNAL_2_STR(SIGTTIN),	/* 21 */
	SIGNAL_2_STR(SIGTTOU),	/* 22 */
	SIGNAL_2_STR(SIGURG),	/* 23 */
	SIGNAL_2_STR(SIGXCPU),	/* 24 */
	SIGNAL_2_STR(SIGXFSZ),	/* 25 */
	SIGNAL_2_STR(SIGVTALRM), /* 26 */
	SIGNAL_2_STR(SIGPROF),	/* 27 */
	SIGNAL_2_STR(SIGWINCH),	/* 28 */
	SIGNAL_2_STR(SIGPOLL),	/* 29 */
	SIGNAL_2_STR(SIGPWR),	/* 30 */
	SIGNAL_2_STR(SIGSYS)	/* 31 */
};
#define SIGNALMAX SIGSYS

#endif
