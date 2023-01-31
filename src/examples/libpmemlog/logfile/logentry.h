/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * info prepended to each log entry...
 */
struct logentry {
	size_t len;		/* length of the rest of the log entry */
	time_t timestamp;
	pid_t pid;

};
