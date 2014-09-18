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
 * vmem_pool_check.c -- unit test for vmem_pool_check
 *
 * usage: vmem_pool_check [directory]
 */

#include "unittest.h"
#include "../../util.h"
#include "../../vmem.h"

static char Mem_pool[VMEM_MIN_POOL*2];

int
main(int argc, char *argv[])
{
	char *dir = NULL;
	VMEM *vmp;

	START(argc, argv, "vmem_pool_check");

	if (argc == 2) {
		dir = argv[1];
	} else if (argc > 2) {
		FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		vmp = vmem_pool_create_in_region(Mem_pool, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create_in_region");
	} else {
		vmp = vmem_pool_create(dir, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create");
	}

	ASSERTeq(0, vmem_pool_check(vmp));

	/* check null addr */
	ASSERTeq(0, vmem_pool_check(vmp));
	void* addr = vmp->addr;
	vmp->addr = NULL;
	ASSERTne(0, vmem_pool_check(vmp));
	vmp->addr = addr;

	/* check wrong size */
	ASSERTeq(0, vmem_pool_check(vmp));
	size_t size = vmp->size;
	vmp->size = 1;
	ASSERTne(0, vmem_pool_check(vmp));
	vmp->size = size;


	/* create pool in this same memory */
	if (dir == NULL) {
		VMEM *vmp2 = vmem_pool_create_in_region(
			(void *)((uintptr_t)Mem_pool + VMEM_MIN_POOL/2),
			VMEM_MIN_POOL);

		if (vmp2 == NULL)
			FATAL("!vmem_pool_create_in_region");

		/* detect memory range collision */
		ASSERTne(0, vmem_pool_check(vmp));
		ASSERTne(0, vmem_pool_check(vmp2));

		vmem_pool_delete(vmp2);

		ASSERTne(0, vmem_pool_check(vmp2));

		/* detect dirty pages after memory corruption by pool vmp2 */
		ASSERTne(0, vmem_pool_check(vmp));
	}

	vmem_pool_delete(vmp);

	/* for vmem_pool_create() memory unmapped after delete pool */
	if (!dir)
		ASSERTne(0, vmem_pool_check(vmp));

	DONE(NULL);
}
