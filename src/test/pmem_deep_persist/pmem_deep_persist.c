/*
 * Copyright 2018-2019, Intel Corporation
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

#ifdef _MSC_VER
/*
 * Since libpmemobj is linked statically, we need to invoke its ctor/dtor.
 */
MSVC_CONSTR(libpmemobj_init)
MSVC_DESTR(libpmemobj_fini)
#endif
