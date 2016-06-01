/*
 * Copyright 2015-2016, Intel Corporation
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
 * obj_constructor.c -- tests for constructor
 */

#include <stddef.h>

#include "unittest.h"

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(constr);
POBJ_LAYOUT_ROOT(constr, struct root);
POBJ_LAYOUT_TOID(constr, struct node);
POBJ_LAYOUT_END(constr);

struct root {
	TOID(struct node) n;
	POBJ_LIST_HEAD(head, struct node) list;
};

struct node {
	POBJ_LIST_ENTRY(struct node) next;
};

static int
root_constr_cancel(PMEMobjpool *pop, void *ptr, void *arg)
{
	return 1;
}

static int
node_constr_cancel(PMEMobjpool *pop, void *ptr, void *arg)
{
	return 1;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_constructor");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(constr) != 1);

	if (argc != 2)
		UT_FATAL("usage: %s file-name", argv[0]);

	const char *path = argv[1];

	PMEMobjpool *pop = NULL;

	int ret;
	TOID(struct root) root;
	TOID(struct node) node;

	if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(constr),
			0, S_IWUSR | S_IRUSR)) == NULL)
		UT_FATAL("!pmemobj_create: %s", path);

	/*
	 * Allocate memory until OOM, so we can check later if the alloc
	 * cancellation didn't damage the heap in any way.
	 */
	int allocs = 0;
	while (pmemobj_alloc(pop, NULL, sizeof(struct node), 1,
			NULL, NULL) == 0)
		allocs++;

	UT_ASSERTne(allocs, 0);

	PMEMoid oid;
	PMEMoid next;
	POBJ_FOREACH_SAFE(pop, oid, next)
		pmemobj_free(&oid);

	errno = 0;
	root.oid = pmemobj_root_construct(pop, sizeof(struct root),
			root_constr_cancel, NULL);
	UT_ASSERT(TOID_IS_NULL(root));
	UT_ASSERTeq(errno, ECANCELED);

	errno = 0;
	ret = pmemobj_alloc(pop, NULL, sizeof(struct node), 1,
			node_constr_cancel, NULL);
	UT_ASSERTeq(ret, -1);
	UT_ASSERTeq(errno, ECANCELED);

	/* the same number of allocations should be possible. */
	while (pmemobj_alloc(pop, NULL, sizeof(struct node), 1,
			NULL, NULL) == 0)
		allocs--;
	UT_ASSERTeq(allocs, 0);
	POBJ_FOREACH_SAFE(pop, oid, next)
		pmemobj_free(&oid);

	root.oid = pmemobj_root_construct(pop, sizeof(struct root),
			NULL, NULL);
	UT_ASSERT(!TOID_IS_NULL(root));

	errno = 0;
	node.oid = pmemobj_list_insert_new(pop, offsetof(struct node, next),
			&D_RW(root)->list, OID_NULL, 0, sizeof(struct node),
			1, node_constr_cancel, NULL);
	UT_ASSERT(TOID_IS_NULL(node));
	UT_ASSERTeq(errno, ECANCELED);

	pmemobj_close(pop);

	DONE(NULL);
}
