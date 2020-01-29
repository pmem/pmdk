// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * blk_rw.c -- unit test for pmemblk_read/write/set_zero/set_error
 *
 * usage: blk_rw bsize file func operation:lba...
 *
 * func is 'c' or 'o' (create or open)
 * operations are 'r' or 'w' or 'z' or 'e'
 *
 */

#include "unittest.h"

static size_t Bsize;

/*
 * construct -- build a buffer for writing
 */
static void
construct(unsigned char *buf)
{
	static int ord = 1;

	for (int i = 0; i < Bsize; i++)
		buf[i] = ord;

	ord++;

	if (ord > 255)
		ord = 1;
}

/*
 * ident -- identify what a buffer holds
 */
static char *
ident(unsigned char *buf)
{
	static char descr[100];
	unsigned val = *buf;

	for (int i = 1; i < Bsize; i++)
		if (buf[i] != val) {
			sprintf(descr, "{%u} TORN at byte %d", val, i);
			return descr;
		}

	sprintf(descr, "{%u}", val);
	return descr;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_rw");

	if (argc < 5)
		UT_FATAL("usage: %s bsize file func op:lba...", argv[0]);

	Bsize = strtoul(argv[1], NULL, 0);

	const char *path = argv[2];

	PMEMblkpool *handle = NULL;
	switch (*argv[3]) {
		case 'c':
			handle = pmemblk_create(path, Bsize, 0,
					S_IWUSR | S_IRUSR);
			if (handle == NULL)
				UT_FATAL("!%s: pmemblk_create", path);
			break;
		case 'o':
			handle = pmemblk_open(path, Bsize);
			if (handle == NULL)
				UT_FATAL("!%s: pmemblk_open", path);
			break;
	}

	UT_OUT("%s block size %zu usable blocks %zu",
			argv[1], Bsize, pmemblk_nblock(handle));

	unsigned char *buf = MALLOC(Bsize);
	if (buf == NULL)
		UT_FATAL("cannot allocate buf");

	/* map each file argument with the given map type */
	for (int arg = 4; arg < argc; arg++) {
		if (strchr("rwze", argv[arg][0]) == NULL || argv[arg][1] != ':')
			UT_FATAL("op must be r: or w: or z: or e:");
		os_off_t lba = strtol(&argv[arg][2], NULL, 0);

		switch (argv[arg][0]) {
		case 'r':
			if (pmemblk_read(handle, buf, lba) < 0)
				UT_OUT("!read      lba %jd", lba);
			else
				UT_OUT("read      lba %jd: %s", lba,
						ident(buf));
			break;

		case 'w':
			construct(buf);
			if (pmemblk_write(handle, buf, lba) < 0)
				UT_OUT("!write     lba %jd", lba);
			else
				UT_OUT("write     lba %jd: %s", lba,
						ident(buf));
			break;

		case 'z':
			if (pmemblk_set_zero(handle, lba) < 0)
				UT_OUT("!set_zero  lba %jd", lba);
			else
				UT_OUT("set_zero  lba %jd", lba);
			break;

		case 'e':
			if (pmemblk_set_error(handle, lba) < 0)
				UT_OUT("!set_error lba %jd", lba);
			else
				UT_OUT("set_error lba %jd", lba);
			break;
		}
	}

	FREE(buf);
	UT_ASSERTne(handle, NULL);
	pmemblk_close(handle);

	int result = pmemblk_check(path, Bsize);
	if (result < 0)
		UT_OUT("!%s: pmemblk_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemblk_check: not consistent", path);

	DONE(NULL);
}
