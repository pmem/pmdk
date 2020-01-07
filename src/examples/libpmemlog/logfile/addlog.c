// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * addlog -- given a log file, append a log entry
 *
 * Usage:
 *	fallocate -l 1G /path/to/pm-aware/file
 *	addlog /path/to/pm-aware/file "first line of entry" "second line"
 */

#include <ex_common.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libpmemlog.h>

#include "logentry.h"

int
main(int argc, char *argv[])
{
	PMEMlogpool *plp;
	struct logentry header;
	struct iovec *iovp;
	struct iovec *next_iovp;
	int iovcnt;

	if (argc < 3) {
		fprintf(stderr, "usage: %s filename lines...\n", argv[0]);
		exit(1);
	}

	const char *path = argv[1];

	/* create the log in the given file, or open it if already created */
	plp = pmemlog_create(path, 0, CREATE_MODE_RW);

	if (plp == NULL &&
	    (plp = pmemlog_open(path)) == NULL) {
		perror(path);
		exit(1);
	}

	/* fill in the header */
	time(&header.timestamp);
	header.pid = getpid();

	/*
	 * Create an iov for pmemlog_appendv().  For each argument given,
	 * allocate two entries (one for the string, one for the newline
	 * appended to the string).  Allocate 1 additional entry for the
	 * header that gets prepended to the entry.
	 */
	iovcnt = (argc - 2) * 2 + 2;
	if ((iovp = malloc(sizeof(*iovp) * iovcnt)) == NULL) {
		perror("malloc");
		exit(1);
	}
	next_iovp = iovp;

	/* put the header into iov first */
	next_iovp->iov_base = &header;
	next_iovp->iov_len = sizeof(header);
	next_iovp++;

	/*
	 * Now put each arg in, following it with the string "\n".
	 * Calculate a total character count in header.len along the way.
	 */
	header.len = 0;
	for (int arg = 2; arg < argc; arg++) {
		/* add the string given */
		next_iovp->iov_base = argv[arg];
		next_iovp->iov_len = strlen(argv[arg]);
		header.len += next_iovp->iov_len;
		next_iovp++;

		/* add the newline */
		next_iovp->iov_base = "\n";
		next_iovp->iov_len = 1;
		header.len += 1;
		next_iovp++;
	}

	/*
	 * pad with NULs (at least one) to align next entry to sizeof(long long)
	 * bytes
	 */
	int a = sizeof(long long);
	int len_to_round = 1 + (a - (header.len + 1) % a) % a;
	char *buf[sizeof(long long)] = {0};
	next_iovp->iov_base = buf;
	next_iovp->iov_len = len_to_round;
	header.len += len_to_round;
	next_iovp++;

	/* atomically add it all to the log */
	if (pmemlog_appendv(plp, iovp, iovcnt) < 0) {
		perror("pmemlog_appendv");
		free(iovp);
		exit(1);
	}

	free(iovp);

	pmemlog_close(plp);
	return 0;
}
