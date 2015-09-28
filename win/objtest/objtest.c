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

#include <stdio.h>
#include <libpmemobj.h>

static char Filename[] = "C:\\temp\\testfile.bin";
static char Layout[] = "objtest";

void libpmem_init(void);
void libpmem_fini(void);
void libpmemobj_init(void);
void libpmemobj_fini(void);

struct myobj {
	PMEMoid next;
	int value;
	char buf[16];
};

struct myroot {
	PMEMoid obj;
};

TOID_DECLARE_ROOT(struct myroot);
TOID_DECLARE(struct myobj, 1);


struct carg {
	int value;
	char *str;
};

void constr(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct carg *a = arg;
	struct myobj *o = ptr;

	o->value = a->value;
	strncpy(o->buf, a->str, sizeof (o->buf));
	o->buf[sizeof (o->buf) - 1] = '\0';
}


int main(int argc, const char *argv[])
{
	/* XXX - library constructors */
	libpmem_init();
	libpmemobj_init();

	const char *msg = pmemobj_check_version(PMEMOBJ_MAJOR_VERSION,
				PMEMOBJ_MINOR_VERSION);
	if (msg) {
		fprintf(stderr, "%s", msg);
		exit(1);
	}

	/* delete old pool file, if exists */
	unlink(Filename);

	PMEMobjpool *pop;
	pop = pmemobj_create(Filename, Layout, PMEMOBJ_MIN_POOL, 0);
	if (pop == NULL)
		exit(1);

	/* non-TX allocation */
	TOID(struct myobj) o;
	int ret = POBJ_ZNEW(pop, &o, struct myobj);
	if (ret)
		exit(1);
	POBJ_FREE(&o);

	/* allocattion with ctor */
	struct carg a = { 1234, "Hello world!" };
	PMEMoid oid;
	ret = pmemobj_alloc(pop, &oid, sizeof (struct myobj), 0, constr, &a);
	if (ret)
		exit(1);

#if 0
	/* TX */
	TOID(struct myroot) root = POBJ_ROOT(pop, sizeof(struct myroot));
	TX_BEGIN(pop) {
		TX_ADD(root);
		D_RW(root)->obj = TX_ZNEW(struct myobj);
		D_RW(D_RW(root)->obj)->value = 5;
	} TX_ONCOMMIT {
		printf("transaction committed\n");
	} TX_END;
#endif

	pmemobj_close(pop);

	/* reopen the pool, and dump the object data */
	pop = pmemobj_open(Filename, Layout);
	if (pop == NULL)
		exit(1);

	oid = pmemobj_first(pop, 0);
	if (OID_IS_NULL(oid)) {
		fprintf(stderr, "can't find any object\n");
		exit(1);
	}

	struct myobj *obj = pmemobj_direct(oid);
	printf("myobj: value = %d\n", obj->value);
	printf("myobj: str = %s\n", obj->buf);

	pmemobj_free(&oid);

	pmemobj_close(pop);

	/* XXX - library destructors */
	libpmemobj_fini();
	libpmem_fini();

	return 0;
}
