// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2023, Intel Corporation */

/*
 * pmem_deep_persist.c -- unit test for pmem_deep_persist()
 *
 * usage: pmem_deep_persist file type deep_persist_size offset
 *
 * type is one of:
 * p - call pmem_map_file()
 * m - call mmap()
 * o - call pmemobj_create()
 */

#include <string.h>
#include "unittest.h"
#include "file.h"
#include "os.h"
#include "file.h"
#include "set.h"
#include "obj.h"
#include "valgrind_internal.h"
#include "pmemcommon.h"
#include "pmem.h"
#define LAYOUT_NAME "deep_persist"

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_deep_persist");

	pmem_init();

	if (argc != 5)
		UT_FATAL("usage: %s file type deep_persist_size offset",
			argv[0]);

	char *addr;
	size_t mapped_len;
	size_t persist_size;
	size_t offset;
	const char *path;
	int is_pmem;
	int ret = -1;

	path = argv[1];
	ssize_t tmp = ATOLL(argv[3]);
	if (tmp < 0)
		persist_size = UINT64_MAX;
	else
		persist_size = (size_t)tmp;

	offset = ATOULL(argv[4]);

	switch (*argv[2]) {
		case 'p':
			if ((addr = pmem_map_file(path, 0, 0,
					0, &mapped_len, &is_pmem)) == NULL) {
				UT_FATAL("!pmem_map_file");
			}

			if (persist_size == UINT64_MAX)
				persist_size = mapped_len;
			ret = pmem_deep_persist(addr + offset, persist_size);

			pmem_unmap(addr, mapped_len);

			break;
		case 'm':
		{
			int fd = OPEN(path, O_RDWR);
			ssize_t size = util_fd_get_size(fd);
			if (size < 0)
				UT_FATAL("!util_fd_get_size: %s", path);
			size_t file_size = (size_t)size;
			/* XXX: add MAP_SYNC flag */
			addr = MMAP(NULL, file_size, PROT_READ|PROT_WRITE,
				MAP_SHARED, fd, 0);
			UT_ASSERTne(addr, MAP_FAILED);
			CLOSE(fd);

			if (persist_size == UINT64_MAX)
				persist_size = file_size;
			ret = pmem_deep_persist(addr + offset, persist_size);

			break;
		}
		case 'o':
		{
			PMEMobjpool *pop = NULL;
			if ((pop = pmemobj_create(path, LAYOUT_NAME,
					0, S_IWUSR | S_IRUSR)) == NULL)
				UT_FATAL("!pmemobj_create: %s", path);

			void *start = (void *)((uintptr_t)pop + offset);
			int flush = 1;
			VALGRIND_DO_MAKE_MEM_DEFINED(start, persist_size);
			ret = util_replica_deep_common(start, persist_size,
					pop->set, 0, flush);
			pmemobj_close(pop);
		}
	}

	UT_OUT("deep_persist %d", ret);

	DONE(NULL);

}
