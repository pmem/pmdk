/*
 * Copyright 2015-2018, Intel Corporation
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
 * obj_redo_log.c -- unit test for redo log
 *
 * usage: obj_redo_log <redo_log_size> [sfrePR][:offset[:value]]
 *
 * s:<index>:<offset>:<value> - store <value> at <offset>
 * f:<index>:<offset>:<value> - store last <value> at <offset>
 * F:<index>                  - set <index> entry as the last one
 * r:<offset>                 - read at <offset>
 * e:<index>                  - read redo log entry at <index>
 * P                          - process redo log
 * R                          - recovery
 * C                          - check  consistency of redo log
 *
 * <offset> and <value> must be in hex
 * <index> must be in dec
 *
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "obj.h"
#include "redo.h"
#include "util.h"
#include "valgrind_internal.h"
#include "set.h"

#include "unittest.h"

#define FATAL_USAGE()	UT_FATAL("usage: obj_redo_log <fname> <redo_log_size> "\
		"[sfFrePRC][<index>:<offset>:<value>]\n")

#define PMEMOBJ_POOL_HDR_SIZE	8192

static void
pmem_drain_nop(void)
{
}

/*
 * obj_persist -- pmemobj version of pmem_persist w/o replication
 */
static void
obj_persist(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = ctx;
	pop->persist_local(addr, len);
}

/*
 * obj_flush -- pmemobj version of pmem_flush w/o replication
 */
static void
obj_flush(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = ctx;
	pop->flush_local(addr, len);
}

/*
 * obj_drain -- pmemobj version of pmem_drain w/o replication
 */
static void
obj_drain(void *ctx)
{
	PMEMobjpool *pop = ctx;
	pop->drain_local();
}

static int
redo_log_check_offset(void *ctx, uint64_t offset)
{
	PMEMobjpool *pop = ctx;
	return OBJ_OFF_IS_VALID(pop, offset);
}

static void
obj_msync_nofail(const void *addr, size_t size)
{
	if (pmem_msync(addr, size))
		UT_FATAL("!pmem_msync");
}

static PMEMobjpool *
pmemobj_open_mock(const char *fname, size_t redo_size)
{
	size_t size;
	int is_pmem;

	void *addr = pmem_map_file(fname, 0, 0, 0, &size, &is_pmem);
	if (!addr) {
		UT_OUT("!%s: pmem_map_file", fname);
		return NULL;
	}

	UT_ASSERT(size >= PMEMOBJ_POOL_HDR_SIZE + redo_size);

	PMEMobjpool *pop = (PMEMobjpool *)addr;
	VALGRIND_REMOVE_PMEM_MAPPING((char *)addr + sizeof(pop->hdr), 4096);
	pop->addr = addr;
	pop->is_pmem = is_pmem;
	pop->rdonly = 0;
	pop->set = MALLOC(sizeof(*pop->set));
	pop->set->poolsize = size;

	if (pop->is_pmem) {
		pop->persist_local = pmem_persist;
		pop->flush_local = pmem_flush;
		pop->drain_local = pmem_drain;
	} else {
		pop->persist_local = obj_msync_nofail;
		pop->flush_local = obj_msync_nofail;
		pop->drain_local = pmem_drain_nop;
	}

	pop->p_ops.persist = obj_persist;
	pop->p_ops.flush = obj_flush;
	pop->p_ops.drain = obj_drain;
	pop->p_ops.base = pop;

	pop->heap_offset = PMEMOBJ_POOL_HDR_SIZE + redo_size;
	pop->heap_size = size - pop->heap_offset;

	pop->redo = redo_log_config_new(pop->addr, &pop->p_ops,
			redo_log_check_offset, pop, REDO_NUM_ENTRIES);

	return pop;
}

static void
pmemobj_close_mock(PMEMobjpool *pop)
{
	redo_log_config_delete(pop->redo);

	size_t poolsize = pop->set->poolsize;
	FREE(pop->set);
	UT_ASSERTeq(pmem_unmap(pop, poolsize), 0);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_redo_log");
	util_init();

	if (argc < 4)
		FATAL_USAGE();

	char *end = NULL;
	errno = 0;
	size_t redo_cnt = strtoul(argv[2], &end, 0);
	if (errno || !end || *end != '\0')
		FATAL_USAGE();

	size_t redo_size = redo_cnt * sizeof(struct redo_log);

	PMEMobjpool *pop = pmemobj_open_mock(argv[1], redo_size);
	UT_ASSERTne(pop, NULL);

	UT_ASSERTeq(util_is_zeroed((char *)pop->addr + PMEMOBJ_POOL_HDR_SIZE,
			pop->set->poolsize - PMEMOBJ_POOL_HDR_SIZE), 1);

	struct redo_log *redo =
		(struct redo_log *)((char *)pop->addr + PMEMOBJ_POOL_HDR_SIZE);

	uint64_t offset;
	uint64_t value;
	int i;
	int ret;
	size_t index;
	for (i = 3; i < argc; i++) {
		char *arg = argv[i];
		UT_ASSERTne(arg, NULL);

		switch (arg[0]) {
		case 's':
			if (sscanf(arg, "s:%zd:0x%zx:0x%zx",
					&index, &offset, &value) != 3)
				FATAL_USAGE();
			UT_OUT("s:%ld:0x%08lx:0x%08lx", index, offset, value);
			redo_log_store(pop->redo, redo, index, offset,
					value);
			break;
		case 'f':
			if (sscanf(arg, "f:%zd:0x%zx:0x%zx",
					&index, &offset, &value) != 3)
				FATAL_USAGE();
			UT_OUT("f:%ld:0x%08lx:0x%08lx", index, offset, value);
			redo_log_store_last(pop->redo, redo, index, offset,
					value);
			break;
		case 'F':
			if (sscanf(arg, "F:%zd", &index) != 1)
				FATAL_USAGE();
			UT_OUT("F:%ld", index);
			redo_log_set_last(pop->redo, redo, index);
			break;
		case 'r':
			if (sscanf(arg, "r:0x%zx", &offset) != 1)
				FATAL_USAGE();

			uint64_t *valp = (uint64_t *)((uintptr_t)pop->addr
					+ offset);
			UT_OUT("r:0x%08lx:0x%08lx", offset, *valp);
			break;
		case 'e':
			if (sscanf(arg, "e:%zd", &index) != 1)
				FATAL_USAGE();

			struct redo_log *entry = redo + index;

			int flag = redo_log_is_last(entry);
			offset = redo_log_offset(entry);
			value = entry->value;

			UT_OUT("e:%ld:0x%08lx:%d:0x%08lx", index, offset,
					flag, value);
			break;
		case 'P':
			redo_log_process(pop->redo, redo, redo_cnt);
			UT_OUT("P");
			break;
		case 'R':
			redo_log_recover(pop->redo, redo, redo_cnt);
			UT_OUT("R");
			break;
		case 'C':
			ret = redo_log_check(pop->redo, redo, redo_cnt);
			UT_OUT("C:%d", ret);
			break;
		case 'n':
			UT_OUT("n:%ld", redo_log_nflags(redo, redo_cnt));
			break;
		default:
			FATAL_USAGE();
		}
	}

	pmemobj_close_mock(pop);

	DONE(NULL);
}
