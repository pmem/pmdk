// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * printlog -- given a log file, print the entries
 *
 * Usage:
 *	printlog [-t] /path/to/pm-aware/file
 *
 * -t option means truncate the file after printing it.
 */

#include <ex_common.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <libpmemlog.h>

#include "logentry.h"

/*
 * printlog -- callback function called when walking the log
 */
static int
printlog(const void *buf, size_t len, void *arg)
{
	/* first byte after log contents */
	const void *endp = (char *)buf + len;

	/* for each entry in the log... */
	while (buf < endp) {
		struct logentry *headerp = (struct logentry *)buf;
		buf = (char *)buf + sizeof(struct logentry);

		/* print the header */
		printf("Entry from pid: %d\n", headerp->pid);
		printf("       Created: %s", ctime(&headerp->timestamp));
		printf("      Contents:\n");

		/* print the log data itself, it is NUL-terminated */
		printf("%s", (char *)buf);
		buf = (char *)buf + headerp->len;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int ind = 1;
	int tflag = 0;
	PMEMlogpool *plp;

	if (argc > 2) {
		if (strcmp(argv[1], "-t") == 0) {
			tflag = 1;
			ind++;
		} else {
			fprintf(stderr, "usage: %s [-t] file\n", argv[0]);
			exit(1);
		}
	}

	const char *path = argv[ind];

	if ((plp = pmemlog_open(path)) == NULL) {
		perror(path);
		exit(1);
	}

	/* the rest of the work happens in printlog() above */
	pmemlog_walk(plp, 0, printlog, NULL);

	if (tflag)
		pmemlog_rewind(plp);

	pmemlog_close(plp);
	return 0;
}
