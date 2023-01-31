// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * manpage.c -- simple example for the libpmemlog man page
 */

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmemlog.h>

/* size of the pmemlog pool -- 1 GB */
#define POOL_SIZE ((size_t)(1 << 30))

/*
 * printit -- log processing callback for use with pmemlog_walk()
 */
static int
printit(const void *buf, size_t len, void *arg)
{
	fwrite(buf, len, 1, stdout);
	return 0;
}

int
main(int argc, char *argv[])
{
	const char path[] = "/pmem-fs/myfile";
	PMEMlogpool *plp;
	size_t nbyte;
	char *str;

	/* create the pmemlog pool or open it if it already exists */
	plp = pmemlog_create(path, POOL_SIZE, 0666);

	if (plp == NULL)
	    plp = pmemlog_open(path);

	if (plp == NULL) {
		perror(path);
		exit(1);
	}

	/* how many bytes does the log hold? */
	nbyte = pmemlog_nbyte(plp);
	printf("log holds %zu bytes\n", nbyte);

	/* append to the log... */
	str = "This is the first string appended\n";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}
	str = "This is the second string appended\n";
	if (pmemlog_append(plp, str, strlen(str)) < 0) {
		perror("pmemlog_append");
		exit(1);
	}

	/* print the log contents */
	printf("log contains:\n");
	pmemlog_walk(plp, 0, printit, NULL);

	pmemlog_close(plp);
	return 0;
}
