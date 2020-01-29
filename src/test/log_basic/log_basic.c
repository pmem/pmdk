// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * log_basic.c -- unit test for pmemlog_*
 *
 * usage: log_basic file operation:...
 *
 * operations are 'l' or 'h' or 'f' or 'c' or 'n' or 'a' or 'v' or 't'
 * or 'r' or 'w'
 *
 */

#include "unittest.h"
#include "../libpmemlog/log.h"

/*
 * do_nbyte -- call pmemlog_nbyte() & print result
 */
static void
do_nbyte(PMEMlogpool *plp)
{
	size_t nbyte = pmemlog_nbyte(plp);
	UT_OUT("usable size: %zu", nbyte);
}

/*
 * do_append -- call pmemlog_append() & print result
 */
static void
do_append(PMEMlogpool *plp)
{
	const char *str[6] = {
		"1st test string\n",
		"2nd test string\n",
		"3rd test string\n",
		"4th test string\n",
		"5th test string\n",
		"6th test string\n"
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
			.iov_base = "1st test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "2nd test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "3rd test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "4th test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "5th test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "6th test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "7th test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "8th test string\n",
			.iov_len = 16
		},
		{
			.iov_base = "9th test string\n",
			.iov_len = 16
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

	rv = pmemlog_appendv(plp, iov, 0);
	UT_ASSERTeq(rv, 0);

	errno = 0;
	rv = pmemlog_appendv(plp, iov, -3);
	UT_ASSERTeq(errno, EINVAL);
	UT_ASSERTeq(rv, -1);
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
 * do_rewind -- call pmemlog_rewind() & print result
 */
static void
do_rewind(PMEMlogpool *plp)
{
	pmemlog_rewind(plp);
	UT_OUT("rewind");
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

	return 1;
}

/*
 * do_walk -- call pmemlog_walk() & print result
 *
 * pmemlog_walk() is called twice: for chunk size 0 and 16
 */
static void
do_walk(PMEMlogpool *plp)
{
	pmemlog_walk(plp, 0, printit, NULL);
	UT_OUT("walk all at once");
	pmemlog_walk(plp, 16, printit, NULL);
	UT_OUT("walk by 16");
}

/*
 * do_create -- call pmemlog_create() and check if it was successful
 */
static PMEMlogpool *
do_create(const char *path)
{
	PMEMlogpool *_plp = pmemlog_create(path, 0, S_IWUSR | S_IRUSR);
	if (_plp == NULL)
		UT_FATAL("!pmemlog_create: %s", path);

	return _plp;
}

/*
 * do_fault_injection -- inject error in first Malloc() in pmemlog_create()
 */
static void
do_fault_injection(PMEMlogpool *plp, const char *path)
{
	if (pmemlog_fault_injection_enabled()) {
		pmemlog_inject_fault_at(PMEM_MALLOC, 1,
					"log_runtime_init");

		plp = pmemlog_create(path, 0, S_IWUSR | S_IRUSR);
		UT_ASSERTeq(plp, NULL);
		UT_ASSERTeq(errno, ENOMEM);
	}
}

/*
 * do_close -- call pmemlog_close()
 */
static void
do_close(PMEMlogpool *plp)
{
	pmemlog_close(plp);
}

/*
 * do_check -- call pmemlog_check() and check consistency
 */
static void
do_check(const char *path)
{
	int result = pmemlog_check(path);
	if (result < 0)
		UT_OUT("!%s: pmemlog_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemlog_check: not consistent", path);
}

int
main(int argc, char *argv[])
{
	PMEMlogpool *plp = NULL;

	START(argc, argv, "log_basic");

	if (argc < 3)
		UT_FATAL("usage: %s file-name op:l|h|f|c|n|a|v|t|r|w", argv[0]);

	const char *path = argv[1];
	/* go through all arguments one by one */
	for (int arg = 2; arg < argc; arg++) {
		/* Scan the character of each argument. */
		if (strchr("lhfcnavtrw", argv[arg][0]) == NULL ||
				argv[arg][1] != '\0')
			UT_FATAL("op must be l or h or f or c or n or a or v\
				or t or r or w");

		switch (argv[arg][0]) {
		case 'c':
			plp = do_create(path);
			break;
		case 'n':
			do_nbyte(plp);
			break;

		case 'a':
			do_append(plp);
			break;

		case 'v':
			do_appendv(plp);
			break;

		case 't':
			do_tell(plp);
			break;

		case 'r':
			do_rewind(plp);
			break;

		case 'w':
			do_walk(plp);
			break;

		case 'f':
			do_fault_injection(plp, path);
			break;
		case 'l':
			UT_ASSERTne(plp, NULL);
			do_close(plp);
			break;
		case 'h':
			do_check(path);
			break;
		}
	}

	DONE(NULL);
}
