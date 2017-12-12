/*
 * Copyright 2015-2017, Intel Corporation
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
 * pmem_memmove.c -- unit test for doing a memmove
 *
 * usage:
 * pmem_memmove file b:length [d:{offset}] [s:offset] [o:{1|2} S:{overlap}]
 *
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"

/*
 * swap_mappings - given to mmapped regions swap them.
 *
 * Try swapping src and dest by unmapping src, mapping a new dest with
 * the original src address as a hint. If successful, unmap original dest.
 * Map a new src with the original dest as a hint.
 * In the event of an error caller must unmap all passed in mappings.
 */
static void
swap_mappings(char **dest, char **src, size_t size, int fd)
{

	char *d = *dest;
	char *s = *src;
	char *ts;
	char *td;

	MUNMAP(*src, size);

	/* mmap destination using src addr as hint */
	td = MMAP(s, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	MUNMAP(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = MMAP(d, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
		-1, 0);
	*src = ts;
}

/*
 * do_memmove: Worker function for memmove.
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest. Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */
static void
do_memmove(int fd, char *dest, char *src, char *file_name, os_off_t dest_off,
	os_off_t src_off, os_off_t off, os_off_t bytes)
{
	void *ret;
	char *src1 = MALLOC(bytes);
	char *buf = MALLOC(bytes);
	char old;

	memset(buf, 0, bytes);
	memset(src1, 0, bytes);
	memset(src, 0x5A, bytes / 4);
	util_persist_auto(util_fd_is_device_dax(fd), src, bytes / 4);
	memset(src + bytes / 4, 0x54, bytes / 4);
	util_persist_auto(util_fd_is_device_dax(fd), src + bytes / 4,
			bytes / 4);

	/* dest == src */
	old = *(char *)(dest + dest_off);
	ret = pmem_memmove_persist(dest + dest_off, dest + dest_off, bytes / 2);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), old);

	/* len == 0 */
	old = *(char *)(dest + dest_off);
	ret = pmem_memmove_persist(dest + dest_off, src + src_off, 0);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), old);

	/*
	 * A side affect of the memmove call is that
	 * src contents will be changed in the case of overlapping
	 * addresses.
	 */
	memcpy(src1, src, bytes / 2);
	ret = pmem_memmove_persist(dest + dest_off, src + src_off, bytes / 2);
	UT_ASSERTeq(ret, dest + dest_off);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src1 + src_off, dest + dest_off, bytes / 2))
		UT_ERR("%s: %zu bytes do not match with memcmp",
			file_name, bytes / 2);

	/*
	 * This is a special case. An overlapping dest means that
	 * src is a pointer to the file, and destination is src + dest_off +
	 * overlap. This is the basis for the comparison. The use of ERR
	 * here is deliberate. This will force a failure of the test but allow
	 * it to continue until its done. The idea is that allowing some
	 * to succeed and others to fail gives more information about what
	 * went wrong.
	 */
	if (dest > src && off != 0) {
		LSEEK(fd, (os_off_t)dest_off + off, SEEK_SET);
		if (READ(fd, buf, bytes / 2) == bytes / 2) {
			if (memcmp(src1 + src_off, buf, bytes / 2))
				UT_ERR("%s: first %zu bytes do not match",
					file_name, bytes / 2);
		}
	} else {
		LSEEK(fd, (os_off_t)dest_off, SEEK_SET);
		if (READ(fd, buf, bytes / 2) == bytes / 2) {
			if (memcmp(src1 + src_off, buf, bytes / 2))
				UT_ERR("%s: first %zu bytes do not match",
					file_name, bytes / 2);
		}
	}
	FREE(src1);
	FREE(buf);
}

#define USAGE() do { UT_FATAL("usage: %s file  b:length [d:{offset}] "\
	"[s:{offset}] [o:{1|2} S:{overlap}]", argv[0]); } while (0)

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	char *src;
	os_off_t dest_off = 0;
	os_off_t src_off = 0;
	uint64_t bytes = 0;
	int who = 0;
	os_off_t overlap = 0;
	size_t mapped_len;

	START(argc, argv, "pmem_memmove");

	fd = OPEN(argv[1], O_RDWR);

	if (argc < 3)
		USAGE();

	for (int arg = 2; arg < argc; arg++) {
		if (strchr("dsboS",
		    argv[arg][0]) == NULL || argv[arg][1] != ':')
			UT_FATAL("op must be d: or s: or b: or o: or S:");

		os_off_t val = strtoul(&argv[arg][2], NULL, 0);

		switch (argv[arg][0]) {
		case 'd':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with d: option",
						val);
			dest_off = val;
			break;

		case 's':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with s: option",
						val);
			src_off = val;
			break;

		case 'b':
			if (val <= 0)
				UT_FATAL("bad length (%lu) with b: option",
						val);
			bytes = val;
			break;

		case 'o':
			if (val != 1 && val != 2)
				UT_FATAL("bad val (%lu) with o: option",
						val);
			who = (int)val;
			break;

		case 'S':
			overlap = val;
			break;
		}
	}

	if (who == 0 && overlap != 0)
		USAGE();

	/* for overlap the src and dest must be created differently */
	if (who == 0) {
		/* src > dest */
		dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dest == NULL)
			UT_FATAL("!could not mmap dest file %s", argv[1]);

		src = MMAP(dest + mapped_len, mapped_len,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
			-1, 0);
		/*
		 * Its very unlikely that src would not be > dest. pmem_map_file
		 * chooses the first unused address >= 1TB, large
		 * enough to hold the give range, and 1GB aligned. Log
		 * the error if the mapped addresses cannot be swapped
		 * but allow the test to continue.
		 */
		if (src <= dest) {
			swap_mappings(&dest, &src, mapped_len, fd);
			if (src <= dest)
				UT_ERR("cannot map files in memory order");
		}

		do_memmove(fd, dest, src, argv[1], dest_off, src_off,
			0, bytes);

		/* dest > src */
		swap_mappings(&dest, &src, mapped_len, fd);

		if (dest <= src)
			UT_ERR("cannot map files in memory order");

		do_memmove(fd, dest, src, argv[1], dest_off, src_off, 0,
			bytes);
		MUNMAP(dest, mapped_len);
		MUNMAP(src, mapped_len);
	} else if (who == 1) {
		/* src overlap with dest */
		dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dest == NULL)
			UT_FATAL("!Could not mmap %s: \n", argv[1]);

		src = dest + overlap;
		memset(dest, 0, bytes);
		util_persist_auto(util_fd_is_device_dax(fd), dest, bytes);
		do_memmove(fd, dest, src, argv[1], dest_off, src_off,
			overlap, bytes);
		MUNMAP(dest, mapped_len);
	} else {
		/* dest overlap with src */
		dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dest == NULL) {
			UT_FATAL("!Could not mmap %s: \n", argv[1]);
		}
		src = dest;
		dest = src + overlap;
		memset(src, 0, bytes);
		util_persist_auto(util_fd_is_device_dax(fd), src, bytes);
		do_memmove(fd, dest, src, argv[1], dest_off, src_off,
			overlap, bytes);
		MUNMAP(src, mapped_len);
	}

	CLOSE(fd);

	DONE(NULL);
}
