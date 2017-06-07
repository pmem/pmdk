/*
 * Copyright 2017, Intel Corporation
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
 * obj_ctl_alloc_class.c -- tests for the ctl entry points: heap.alloc_class
 */

#include <sys/resource.h>
#include "unittest.h"

#define LAYOUT "obj_ctl_alloc_class"

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_ctl_alloc_class");

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop;

	if ((pop = pmemobj_create(path, LAYOUT, PMEMOBJ_MIN_POOL,
		S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	int ret;
	PMEMoid oid;
	size_t usable_size;

	struct pobj_alloc_class_desc alloc_class_128;
	alloc_class_128.header_type = POBJ_HEADER_NONE;
	alloc_class_128.unit_size = 128;
	alloc_class_128.units_per_block = 1000;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.128.desc",
		&alloc_class_128);
	UT_ASSERTeq(ret, 0);

	struct pobj_alloc_class_desc alloc_class_129;
	alloc_class_129.header_type = POBJ_HEADER_COMPACT;
	alloc_class_129.unit_size = 1024;
	alloc_class_129.units_per_block = 1000;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.129.desc",
		&alloc_class_129);
	UT_ASSERTeq(ret, 0);

	struct pobj_alloc_class_desc alloc_class_128_r;
	ret = pmemobj_ctl_get(pop, "heap.alloc_class.128.desc",
		&alloc_class_128_r);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(alloc_class_128.header_type, alloc_class_128_r.header_type);
	UT_ASSERTeq(alloc_class_128.unit_size, alloc_class_128_r.unit_size);
	UT_ASSERT(alloc_class_128.units_per_block <=
		alloc_class_128_r.units_per_block);

	/*
	 * One unit from alloc class 128 - 128 bytes unit size, minimal headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, 128, 0, POBJ_CLASS_ID(128), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 128);
	pmemobj_free(&oid);

	/*
	 * One unit from alloc class 128 - 128 bytes unit size, minimal headers,
	 * but request size 1 byte.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1, 0, POBJ_CLASS_ID(128), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 128);
	pmemobj_free(&oid);

	/*
	 * Two units from alloc class 129 -
	 * 1024 bytes unit size, compact headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1024 + 1,
		0, POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, (1024 * 2) - 16); /* 2 units minus hdr */
	pmemobj_free(&oid);

	/*
	 * 64 units from alloc class 129
	 * - 1024 bytes unit size, compact headers.
	 */
	ret = pmemobj_xalloc(pop, &oid, (1024 * 64) - 16,
		0, POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, 0);

	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, (1024 * 64) - 16);
	pmemobj_free(&oid);

	/*
	 * 65 units from alloc class 129 -
	 * 1024 bytes unit size, compact headers.
	 * Should fail, as it would require two bitmap modifications.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1024 * 64 + 1, 0,
		POBJ_CLASS_ID(129), NULL, NULL);
	UT_ASSERTeq(ret, -1);

	/*
	 * Nonexistent alloc class.
	 */
	ret = pmemobj_xalloc(pop, &oid, 1, 0, POBJ_CLASS_ID(130), NULL, NULL);
	UT_ASSERTeq(ret, -1);

	struct pobj_alloc_class_desc alloc_class_new;
	alloc_class_new.header_type = POBJ_HEADER_NONE;
	alloc_class_new.unit_size = 777;
	alloc_class_new.units_per_block = 200;
	alloc_class_new.class_id = 0;

	ret = pmemobj_ctl_set(pop, "heap.alloc_class.new.desc",
		&alloc_class_new);
	UT_ASSERTeq(ret, 0);

	ret = pmemobj_xalloc(pop, &oid, 1, 0,
		POBJ_CLASS_ID(alloc_class_new.class_id), NULL, NULL);
	UT_ASSERTeq(ret, 0);
	usable_size = pmemobj_alloc_usable_size(oid);
	UT_ASSERTeq(usable_size, 777);

	pmemobj_close(pop);

	DONE(NULL);
}
