// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * checksum.c -- unit test for library internal checksum routine
 *
 * usage: checksum files...
 */

#include <endian.h>
#include "unittest.h"
#include "util.h"
#include <inttypes.h>

/*
 * fletcher64 -- compute a Fletcher64 checksum
 *
 * Gold standard implementation used to compare to the
 * util_checksum() being unit tested.
 */
static uint64_t
fletcher64(void *addr, size_t len)
{
	UT_ASSERT(len % 4 == 0);
	uint32_t *p32 = addr;
	uint32_t *p32end = (uint32_t *)((char *)addr + len);
	uint32_t lo32 = 0;
	uint32_t hi32 = 0;

	while (p32 < p32end) {
		lo32 += le32toh(*p32);
		p32++;
		hi32 += lo32;
	}

	return htole64((uint64_t)hi32 << 32 | lo32);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "checksum");

	if (argc < 2)
		UT_FATAL("usage: %s files...", argv[0]);

	for (int arg = 1; arg < argc; arg++) {
		int fd = OPEN(argv[arg], O_RDONLY);

		os_stat_t stbuf;
		FSTAT(fd, &stbuf);
		size_t size = (size_t)stbuf.st_size;

		void *addr =
			MMAP(NULL, size, PROT_READ|PROT_WRITE,
					MAP_PRIVATE, fd, 0);

		uint64_t *ptr = addr;

		/*
		 * Loop through, selecting successive locations
		 * where the checksum lives in this block, and
		 * let util_checksum() insert it so it can be
		 * verified against the gold standard fletcher64
		 * routine in this file.
		 */
		while ((char *)(ptr + 1) < (char *)addr + size) {
			/* save whatever was at *ptr */
			uint64_t oldval = *ptr;

			/* mess with it */
			*ptr = htole64(0x123);

			/*
			 * calculate a checksum and have it installed
			 */
			util_checksum(addr, size, ptr, 1, 0);

			uint64_t csum = *ptr;

			/*
			 * verify inserted checksum checks out
			 */
			UT_ASSERT(util_checksum(addr, size, ptr, 0, 0));

			/* put a zero where the checksum was installed */
			*ptr = 0;

			/* calculate a checksum */
			uint64_t gold_csum = fletcher64(addr, size);

			/* put the old value back */
			*ptr = oldval;

			/*
			 * verify checksum now fails
			 */
			UT_ASSERT(!util_checksum(addr, size, ptr,
					0, 0));

			/*
			 * verify the checksum matched the gold version
			 */
			UT_ASSERTeq(csum, gold_csum);
			UT_OUT("%s:%" PRIu64 " 0x%" PRIx64, argv[arg],
				(char *)ptr - (char *)addr, csum);

			ptr++;
		}

		uint64_t *addr2 =
			MMAP(NULL, size, PROT_READ|PROT_WRITE,
				MAP_PRIVATE, fd, 0);

		uint64_t *csum = (uint64_t *)addr;

		/*
		 * put a zero where the checksum will be installed
		 * in the second map
		 */
		*addr2 = 0;
		for (size_t i = size / 8 - 1; i > 0; i -= 1) {
			/* calculate a checksum and have it installed */
			util_checksum(addr, size, csum, 1, i * 8);

			/*
			 * put a zero in the second map where an ignored part is
			 */
			*(addr2 + i) = 0;

			/* calculate a checksum */
			uint64_t gold_csum = fletcher64(addr2, size);
			/*
			 * verify the checksum matched the gold version
			 */
			UT_ASSERTeq(*csum, gold_csum);
		}

		CLOSE(fd);
		MUNMAP(addr, size);
		MUNMAP(addr2, size);

	}

	DONE(NULL);
}
