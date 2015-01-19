/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * blk_layout.c -- unit test for the extended read_layout functionality
 * usage: blk_layout bsize file operation...
 *
 * operations are 'f' or 's' or 'b' or 'd'
 * f - invalidate primary BTT Info header
 * s - invalidate backup BTT Info header
 * b - invalidate both
 * d - both valid but different
 */

#define	_GNU_SOURCE
#include "unittest.h"
#include "btt_layout.h"
#include <sys/param.h>
#include <util.h>
#include "blk.h"

size_t block_size;
static const char *file_name;

/* describes the arena layout of the pool */
struct pool_descr {
	unsigned int num_arenas;
	/* offsets of btt_info blocks within the pool */
	struct arena_descr {
		off_t first_info;
		off_t backup_info;
	} *arenas;
};

/*
 * get_pool_info -- calculates essential information about the pool
 */
static void
get_pool_info(size_t pool_size, struct pool_descr *pool)
{
	size_t blk_header_size = roundup(sizeof (struct pmemblk),
			BLK_FORMAT_DATA_ALIGN);
	size_t pool_data_size = pool_size - blk_header_size;
	pool->num_arenas = pool_data_size / BTT_MAX_ARENA;
	uint64_t last_arena_size = pool_data_size % BTT_MAX_ARENA;
	if (last_arena_size >= BTT_MIN_SIZE)
		++(pool->num_arenas);
	off_t first_info_off = blk_header_size;
	pool->arenas = CALLOC(pool->num_arenas, sizeof (struct arena_descr));

	for (int i = 0; i < pool->num_arenas; ++i) {
		uint64_t arena_size = BTT_MAX_ARENA;
		if (i == (pool->num_arenas - 1)) {
			arena_size = last_arena_size ? : BTT_MAX_ARENA;
		}
		pool->arenas[i].first_info = first_info_off;
		pool->arenas[i].backup_info = first_info_off + arena_size -
				sizeof (struct btt_info);
		first_info_off += arena_size;
	}
}

/*
 * check_consistency -- check consistency of the tested pmemblk pool
 */
static void
check_consistency()
{
	int result = pmemblk_check(file_name);
	if (result < 0)
		OUT("!%s: pmemblk_check", file_name);
	else if (result == 0)
		OUT("%s: pmemblk_check: not consistent", file_name);
	else
		OUT("%s: pmemblk_check: consistent", file_name);
}

/*
 * read_info -- read the btt_info from a given offset
 */
static void
read_info(off_t offset, struct btt_info *info)
{
	int fd = OPEN(file_name, O_RDONLY);

	LSEEK(fd, offset, SEEK_SET);
	READ(fd, info, sizeof (*info));
	CLOSE(fd);
}

/*
 * write_info -- write the btt_info at a given offset
 */
static void
write_info(off_t offset, const struct btt_info *info)
{
	int fd = OPEN(file_name, O_RDWR);

	LSEEK(fd, offset, SEEK_SET);
	WRITE(fd, info, sizeof (*info));
	CLOSE(fd);
}

int
main(int argc, char *argv[])
{
	/* set up the test */
	START(argc, argv, "blk_layout");

	if (argc < 4)
		FATAL("usage: %s bsize file op", argv[0]);

	block_size = strtoul(argv[1], NULL, 0);

	file_name = argv[2];

	PMEMblkpool *handle;

	if ((handle = pmemblk_create(file_name, block_size, 0,
			S_IWUSR)) == NULL)
		FATAL("!%s: pmemblk_create", file_name);

	/* write out the layout */
	pmemblk_set_error(handle, 0);

	pmemblk_close(handle);
	/* initialize buffer */
	unsigned char *buf;
	buf = CALLOC(block_size, sizeof (unsigned char));

	struct stat file_stat;
	STAT(file_name, &file_stat);

	struct pool_descr poold;
	get_pool_info(file_stat.st_size, &poold);

	/* num_arenas set by pmemblk_open */
	for (int i = 0; i < poold.num_arenas; ++i) {
		OUT("Testing arena %d", i);
		/* prepare necessary data */
		struct btt_info original;
		read_info(poold.arenas[i].first_info, &original);
		/* map each file argument with the given map type */
		for (int arg = 3; arg < argc; arg++) {
			if (strchr("fsdb", argv[arg][0]) == NULL)
				FATAL("op must be one of: f, s, d, b");
			/* make invalid info copy */
			struct btt_info invalid_info = original;
			invalid_info.external_nlba += 1;

			OUT("Testing op %c", argv[arg][0]);
			/* do requested operation */
			switch (argv[arg][0]) {
				case 'f':
					/* spoil first info */
					write_info(poold.arenas[i].first_info,
							&invalid_info);
					break;
				case 's':
					/* spoil backup info */
					write_info(poold.arenas[i].backup_info,
							&invalid_info);
					break;
				case 'd':
					util_checksum(&invalid_info,
							sizeof (invalid_info),
							&invalid_info.checksum,
							1);
					/* spoil backup info */
					write_info(poold.arenas[i].backup_info,
							&invalid_info);
					break;
				case 'b':
					/* spoil first info */
					write_info(poold.arenas[i].first_info,
							&invalid_info);
					/* spoil backup info */
					write_info(poold.arenas[i].backup_info,
							&invalid_info);
					break;
				default:
					FATAL("unrecognized operation type");
			}

			check_consistency();

			if ((handle = pmemblk_open(file_name,
					block_size)) == NULL)
				FATAL("!%s: pmemblk_create", file_name);

			/* attempt to read from error and non error block */
			if (pmemblk_read(handle, buf, 0))
				OUT("!read lba 0 failed");
			if (pmemblk_read(handle, buf, 1))
				OUT("!read lba 1 failed");
			/* attempt to write in the invalid arena */
			if ((argv[arg][0] == 'b') &&
					(i != poold.num_arenas - 1)) {
				uint64_t write_lba = i + 1;
				write_lba *= (pmemblk_nblock(handle));
				write_lba /= poold.num_arenas;
				write_lba -= poold.num_arenas;
				if (pmemblk_write(handle, buf, write_lba))
					OUT("!write failed");
			}

			pmemblk_close(handle);

			/* revert to valid layout */
			write_info(poold.arenas[i].first_info, &original);
			write_info(poold.arenas[i].backup_info, &original);
		}
	}

	DONE(NULL);
}
