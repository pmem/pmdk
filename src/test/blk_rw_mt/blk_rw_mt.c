/*
 * Copyright 2014-2016, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * blk_rw_mt.c -- unit test for multi-threaded random I/O
 *
 * usage: blk_rw_mt bsize file seed nthread nops
 *
 */

#include "unittest.h"

size_t Bsize;
size_t Nblock = 100;	/* all I/O below this LBA (increases collisions) */
unsigned Seed;
unsigned Nthread;
unsigned Nops;
PMEMblkpool *Handle;

/*
 * construct -- build a buffer for writing
 */
static void
construct(int *ordp, unsigned char *buf)
{
	for (int i = 0; i < Bsize; i++)
		buf[i] = *ordp;

	(*ordp)++;

	if (*ordp > 255)
		*ordp = 1;
}

/*
 * check -- check for torn buffers
 */
static void
check(unsigned char *buf)
{
	unsigned val = *buf;

	for (int i = 1; i < Bsize; i++)
		if (buf[i] != val) {
			UT_OUT("{%u} TORN at byte %d", val, i);
			break;
		}
}

/*
 * worker -- the work each thread performs
 */
static void *
worker(void *arg)
{
	long mytid = (long)(intptr_t)arg;
	unsigned myseed = Seed + mytid;
	unsigned char *buf = MALLOC(Bsize);
	int ord = 1;

	for (unsigned i = 0; i < Nops; i++) {
		off_t lba = rand_r(&myseed) % Nblock;

		if (rand_r(&myseed) % 2) {
			/* read */
			if (pmemblk_read(Handle, buf, lba) < 0)
				UT_OUT("!read      lba %zu", lba);
			else
				check(buf);
		} else {
			/* write */
			construct(&ord, buf);
			if (pmemblk_write(Handle, buf, lba) < 0)
				UT_OUT("!write     lba %zu", lba);
		}
	}

	FREE(buf);

	return NULL;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "blk_rw_mt");

	if (argc != 6)
		UT_FATAL("usage: %s bsize file seed nthread nops", argv[0]);

	Bsize = strtoul(argv[1], NULL, 0);

	const char *path = argv[2];

	if ((Handle = pmemblk_create(path, Bsize, 0,
			S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!%s: pmemblk_create", path);

	if (Nblock == 0)
		Nblock = pmemblk_nblock(Handle);
	Seed = strtoul(argv[3], NULL, 0);
	Nthread = strtoul(argv[4], NULL, 0);
	Nops = strtoul(argv[5], NULL, 0);

	UT_OUT("%s block size %zu usable blocks %zu", argv[1], Bsize, Nblock);

	pthread_t *threads = MALLOC(Nthread * sizeof(pthread_t));

	/* kick off nthread threads */
	for (unsigned i = 0; i < Nthread; i++)
		PTHREAD_CREATE(&threads[i], NULL, worker, (void *)(intptr_t)i);

	/* wait for all the threads to complete */
	for (unsigned i = 0; i < Nthread; i++)
		PTHREAD_JOIN(threads[i], NULL);

	FREE(threads);
	pmemblk_close(Handle);

	/* XXX not ready to pass this part of the test yet */
	int result = pmemblk_check(path, Bsize);
	if (result < 0)
		UT_OUT("!%s: pmemblk_check", path);
	else if (result == 0)
		UT_OUT("%s: pmemblk_check: not consistent", path);

	DONE(NULL);
}
