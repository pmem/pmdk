/*
 * Copyright 2015-2019, Intel Corporation
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
 * blk_non_zero.c -- unit test for pmemblk_read/write/set_zero/
 * set_error/inject_fault
 *
 * usage: blk_non_zero bsize file func operation:lba...
 *
 * func is 'c' or 'o' (create or open)
 * operations are 'r' or 'w' or 'z' or 'e' or 't' ot 'm'
 *
 */
#define _GNU_SOURCE

#include <sys/param.h>
#include "unittest.h"

#include "blk.h"

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

/*
 * is_zeroed -- read is_zeroed flag from header
 */
static int
is_zeroed(const char *path)
{
	int fd = OPEN(path, O_RDWR);
	os_stat_t stbuf;

	FSTAT(fd, &stbuf);

	void *addr = MMAP(NULL, (size_t)stbuf.st_size, PROT_READ|PROT_WRITE,
			MAP_SHARED, fd, 0);

	struct pmemblk *header = addr;

	int ret = header->is_zeroed;

	MUNMAP(addr, (size_t)stbuf.st_size);
	CLOSE(fd);

	return ret;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_non_zero");

	if (argc < 5)
		UT_FATAL("usage: %s bsize file func [file_size] op:lba...",
				argv[0]);

	int read_arg = 1;

	Bsize = strtoul(argv[read_arg++], NULL, 0);

	const char *path = argv[read_arg++];

	PMEMblkpool *handle = NULL;
	switch (*argv[read_arg++]) {
		case 'c': {
			size_t fsize = strtoul(argv[read_arg++], NULL, 0);
			handle = pmemblk_create(path, Bsize, fsize,
					S_IRUSR | S_IWUSR);
			if (handle == NULL)
				UT_FATAL("!%s: pmemblk_create", path);
			break;
		}
		case 'o':
			handle = pmemblk_open(path, Bsize);
			if (handle == NULL)
				UT_FATAL("!%s: pmemblk_open", path);
			break;
		default:
			UT_FATAL("unrecognized command %s", argv[read_arg - 1]);
	}

	UT_OUT("%s block size %zu usable blocks %zu",
			argv[1], Bsize, pmemblk_nblock(handle));

	UT_OUT("is zeroed:\t%d", is_zeroed(path));

	unsigned char *buf = MALLOC(Bsize);
	if (buf == NULL)
		UT_FATAL("cannot allocate buf");

	/* map each file argument with the given map type */
	for (; read_arg < argc; read_arg++) {
		if (strchr("rwzetm", argv[read_arg][0]) == NULL ||
				argv[read_arg][1] != ':')
			UT_FATAL("op must be r: or w: or z: or e: or t: or m:");
		os_off_t lba = STRTOL(&argv[read_arg][2], NULL, 0);

		switch (argv[read_arg][0]) {
		case 'r':
			if (pmemblk_read(handle, buf, lba) < 0)
				UT_OUT("!read      lba %zu", lba);
			else
				UT_OUT("read      lba %zu: %s", lba,
						ident(buf));
			break;

		case 'w':
			construct(buf);
			if (pmemblk_write(handle, buf, lba) < 0)
				UT_OUT("!write     lba %zu", lba);
			else
				UT_OUT("write     lba %zu: %s", lba,
						ident(buf));
			break;

		case 'z':
			if (pmemblk_set_zero(handle, lba) < 0)
				UT_OUT("!set_zero  lba %zu", lba);
			else
				UT_OUT("set_zero  lba %zu", lba);
			break;

		case 'e':
			if (pmemblk_set_error(handle, lba) < 0)
				UT_OUT("!set_error lba %zu", lba);
			else
				UT_OUT("set_error lba %zu", lba);
			break;

		case 't':
			if (!pmemblk_fault_injection_enabled())
				break;

			pmemblk_inject_fault_at(PMEM_MALLOC, 1, "build_rtt");
			int ret = pmemblk_set_error(handle, lba);
			UT_ASSERTne(ret, 0);
			UT_ASSERTeq(errno, ENOMEM);
			break;
		case 'm':
			if (!pmemblk_fault_injection_enabled())
				break;

			pmemblk_inject_fault_at(PMEM_MALLOC, 1,
					"build_map_locks");
			ret = pmemblk_set_error(handle, lba);
			UT_ASSERTne(ret, 0);
			UT_ASSERTeq(errno, ENOMEM);
			break;
		}
	}

	FREE(buf);

	pmemblk_close(handle);

	int result = pmemblk_check(path, Bsize);
	if (result < 0)
		UT_OUT("!%s: pmemblk_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemblk_check: not consistent", path);

	DONE(NULL);
}
