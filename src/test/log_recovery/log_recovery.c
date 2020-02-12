// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * log_recovery.c -- unit test for pmemlog recovery
 *
 * usage: log_recovery file operation:...
 *
 * operation has to be 'a' or 'v'
 *
 */

#include <sys/param.h>
#include "unittest.h"
#include "log.h"

/*
 * do_append -- call pmemlog_append() & print result
 */
static void
do_append(PMEMlogpool *plp)
{
	const char *str[6] = {
		"1st append string\n",
		"2nd append string\n",
		"3rd append string\n",
		"4th append string\n",
		"5th append string\n",
		"6th append string\n"
	};

	for (int i = 0; i < 6; ++i) {
		int rv = pmemlog_append(plp, str[i], strlen(str[i]));
		switch (rv) {
		case 0:
			UT_OUT("append   str[%i] %s", i, str[i]);
			break;
		case -1:
			UT_OUT("!append   str[%i] %s", i, str[i]);
			break;
		default:
			UT_OUT("!append: wrong return value");
			break;
		}
	}
}

/*
 * do_appendv -- call pmemlog_appendv() & print result
 */
static void
do_appendv(PMEMlogpool *plp)
{
	struct iovec iov[9] = {
		{
			.iov_base = "1st appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "2nd appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "3rd appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "4th appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "5th appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "6th appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "7th appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "8th appendv string\n",
			.iov_len = 19
		},
		{
			.iov_base = "9th appendv string\n",
			.iov_len = 19
		}
	};

	int rv = pmemlog_appendv(plp, iov, 9);
	switch (rv) {
	case 0:
		UT_OUT("appendv");
		break;
	case -1:
		UT_OUT("!appendv");
		break;
	default:
		UT_OUT("!appendv: wrong return value");
		break;
	}
}

/*
 * do_tell -- call pmemlog_tell() & print result
 */
static void
do_tell(PMEMlogpool *plp)
{
	os_off_t tell = pmemlog_tell(plp);
	UT_OUT("tell %zu", tell);
}

/*
 * printit -- print out the 'buf' of length 'len'.
 *
 * It is a walker function for pmemlog_walk
 */
static int
printit(const void *buf, size_t len, void *arg)
{
	char *str = MALLOC(len + 1);

	strncpy(str, buf, len);
	str[len] = '\0';
	UT_OUT("%s", str);

	FREE(str);
	return 0;
}

/*
 * do_walk -- call pmemlog_walk() & print result
 */
static void
do_walk(PMEMlogpool *plp)
{
	pmemlog_walk(plp, 0, printit, NULL);
	UT_OUT("walk all at once");
}

static ut_jmp_buf_t Jmp;

/*
 * signal_handler -- called on SIGSEGV
 */
static void
signal_handler(int sig)
{
	UT_OUT("signal: %s", os_strsignal(sig));

	ut_siglongjmp(Jmp);
}

int
main(int argc, char *argv[])
{
	PMEMlogpool *plp;
	int result;

	START(argc, argv, "log_recovery");

	if (argc != 3)
		UT_FATAL("usage: %s file-name op:a|v", argv[0]);

	if (strchr("av", argv[2][0]) == NULL || argv[2][1] != '\0')
		UT_FATAL("op must be a or v");

	const char *path = argv[1];

	int fd = OPEN(path, O_RDWR);

	/* pre-allocate 2MB of persistent memory */
	POSIX_FALLOCATE(fd, (os_off_t)0, (size_t)(2 * 1024 * 1024));

	CLOSE(fd);

	if ((plp = pmemlog_create(path, 0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemlog_create: %s", path);

	/* append some data */
	if (argv[2][0] == 'a')
		do_append(plp);
	else
		do_appendv(plp);

	/* print out current write point */
	do_tell(plp);

	size_t len = roundup(sizeof(*plp), LOG_FORMAT_DATA_ALIGN);
	UT_OUT("write-protecting the metadata, length %zu", len);
	MPROTECT(plp, len, PROT_READ);

	/* arrange to catch SEGV */
	struct sigaction v;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;
	SIGACTION(SIGSEGV, &v, NULL);

	if (!ut_sigsetjmp(Jmp)) {
		/* try to append more data */
		if (argv[2][0] == 'a')
			do_append(plp);
		else
			do_appendv(plp);
	}

	MPROTECT(plp, len, PROT_READ | PROT_WRITE);
	pmemlog_close(plp);

	/* check consistency */
	result = pmemlog_check(path);
	if (result < 0)
		UT_OUT("!%s: pmemlog_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemlog_check: not consistent", path);
	else
		UT_OUT("%s: consistent", path);

	/* map again to print out whole log */
	if ((plp = pmemlog_open(path)) == NULL)
		UT_FATAL("!pmemlog_open: %s", path);

	/* print out current write point */
	do_tell(plp);

	/* print out whole log */
	do_walk(plp);

	pmemlog_close(plp);

	DONE(NULL);
}
