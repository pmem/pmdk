/*
 * Copyright (c) 2015, Intel Corporation
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
 * objtest.c -- simple obj example
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <libpmem.h>
#include <libpmemobj.h>

static char Filename[] = "C:\\temp\\testfile.bin";
static char Layout[] = "objtest";

#ifdef __cplusplus
extern "C" {
#endif
void libpmem_init(void);
void libpmem_fini(void);
void libpmemobj_init(void);
void libpmemobj_fini(void);
#ifdef __cplusplus
}
#endif

TOID_DECLARE_ROOT(struct myroot);
TOID_DECLARE(struct myobj, 1);

struct myobj {
	PMEMoid next;
	int value;
	char buf[16];
};

struct myroot {
	TOID(struct myobj) obj;
	char buf[16];
};


struct carg {
	int value;
	char *str;
};

void
constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct carg *a = (struct carg *)arg;
	struct myobj *o = (struct myobj *)ptr;

	o->value = a->value;
	strncpy(o->buf, a->str, sizeof (o->buf));
	o->buf[sizeof (o->buf) - 1] = '\0';
}


void
test_nontx(PMEMobjpool *pop)
{
	PMEMoid oid;
	struct carg a;

	/* allocations with ctor */
	a.value = 5555;
	a.str = "non-tx alloc";
	if (pmemobj_alloc(pop, &oid, sizeof (struct myobj), 5, constr, &a))
		exit(1);

	if (pmemobj_realloc(pop, &oid, sizeof (struct myobj), 10))
		exit(1);

	if (pmemobj_zalloc(pop, &oid, sizeof (struct myobj), 10))
		exit(1);

	if (pmemobj_zrealloc(pop, &oid, 10 * sizeof (struct myobj), 11))
		exit(1);
}

/* non-TX allocation w/ macros */
void
test_nontx_macros(PMEMobjpool *pop)
{
	TOID(struct myobj) toid;
	struct carg a;

	/* allocations with ctor */
	a.value = 1234;
	a.str = "Hello world!";
	if (POBJ_NEW(pop, &toid, struct myobj, constr, &a))
		exit(1);

	a.value = 4321;
	a.str = "World hello!";
	if (POBJ_ALLOC(pop, &toid, struct myobj, 5 * sizeof (struct myobj),
				constr, &a))
		exit(1);

	if (POBJ_ZNEW(pop, &toid, struct myobj))
		exit(1);

	if (POBJ_ZALLOC(pop, &toid, struct myobj, 10 * sizeof (struct myobj)))
		exit(1);

	if (POBJ_ZREALLOC(pop, &toid, struct myobj,
					20 * sizeof (struct myobj)))
		exit(1);
}

void
test_tx(PMEMobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct myroot));
	TX_BEGIN(pop) {
		pmemobj_tx_add_range(root, offsetof (struct myroot, obj),
					sizeof (struct myroot));
		struct myroot *rootp = (struct myroot *)pmemobj_direct(root);
		rootp->obj.oid = pmemobj_tx_zalloc(sizeof (struct myobj), 6);
		struct myobj *objp =
			(struct myobj *)pmemobj_direct(rootp->obj.oid);
		objp->value = 66;
		strncpy(objp->buf, "sixty six", sizeof (objp->buf));
		pmemobj_tx_add_range_direct(rootp->buf, sizeof (rootp->buf));
		strncpy(rootp->buf, "I'm root object", sizeof (rootp->buf));
	} TX_ONCOMMIT {
		printf("transaction committed\n");
	} TX_END;
}

#ifdef __cplusplus
void
test_tx_macros(PMEMobjpool *pop)
{
	TOID(struct myroot) root; // = POBJ_ROOT(pop, sizeof(struct myroot));
	TX_BEGIN(pop) {
		TX_ADD(root);
		struct myroot *rootp = D_RW(root);
		D_RW(root)->obj =
			TX_REALLOC(D_RO(root)->obj, 3 * sizeof (struct myobj));
		D_RW(D_RW(root)->obj)->value = 777;
	} TX_ONCOMMIT {
		printf("transaction committed\n");
	} TX_END;
}
#endif

#ifdef __cplusplus
void
test_root(PMEMobjpool *pop)
{
	TOID(struct myroot) root = pmemobj_root(pop, sizeof(struct myroot));
	size_t rs = pmemobj_root_size(pop);
	printf("root size = %zu\n", rs);
	strncpy(D_RW(root)->buf, "I'm root object", sizeof (D_RW(root)->buf));
	pmem_memcpy_persist(D_RW(root)->buf,  "I'm root object", 16);
}
#else
void
test_root(PMEMobjpool *pop)
{
	PMEMoid root = pmemobj_root(pop, sizeof(struct myroot));
	size_t rs = pmemobj_root_size(pop);
	printf("root size = %zu\n", rs);
	struct myroot *rootp = pmemobj_direct(root);
	strncpy(rootp->buf, "I'm root object", sizeof (rootp->buf));
	pmem_memcpy_persist(rootp->buf,  "I'm root object", 16);
}
#endif

int
main(int argc, const char *argv[])
{
	/* XXX - library constructors */
	libpmem_init();
	libpmemobj_init();

	const char *msg = pmem_check_version(PMEM_MAJOR_VERSION,
				PMEM_MINOR_VERSION);
	if (msg) {
		fprintf(stderr, "%s", msg);
		exit(1);
	}

	msg = pmemobj_check_version(PMEMOBJ_MAJOR_VERSION,
				PMEMOBJ_MINOR_VERSION);
	if (msg) {
		fprintf(stderr, "%s", msg);
		exit(1);
	}

	/* delete old pool file, if exists */
	unlink(Filename);

	PMEMobjpool *pop;
	pop = pmemobj_create(Filename, Layout, 10 * PMEMOBJ_MIN_POOL, 0);
	if (pop == NULL)
		exit(1);

	test_root(pop);
	test_nontx(pop);
	test_nontx_macros(pop);
	test_tx(pop);
#ifdef __cplusplus
	test_tx_macros(pop);
#endif

	pmemobj_close(pop);

	/* reopen the pool, and dump the object data */
	pop = pmemobj_open(Filename, Layout);
	if (pop == NULL)
		exit(1);

	test_root(pop);

	struct myobj *obj;
	PMEMoid oid;
	PMEMoid noid;
	int type_num;
	POBJ_FOREACH_SAFE(pop, oid, noid, type_num) {
		obj = (struct myobj *)pmemobj_direct(oid);

		printf("myobj: value = %d\n", obj->value);
		printf("myobj: str = %s\n", obj->buf);
		printf("myobj: type num = %d (%d)\n",
					pmemobj_type_num(oid), type_num);
		printf("myobj: usable size = %zu\n",
					pmemobj_alloc_usable_size(oid));

		pmemobj_free(&oid);
	}

	pmemobj_close(pop);

	/* XXX - library destructors */
	libpmemobj_fini();
	libpmem_fini();

	return 0;
}
