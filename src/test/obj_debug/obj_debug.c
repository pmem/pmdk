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
 * obj_debug.c -- unit test for debug features
 *
 * usage: obj_debug file operation:...
 *
 * operations are 'f' or 'l'
 *
 */
#include <stddef.h>
#include <sys/param.h>

#include "unittest.h"
#include "libpmemobj.h"

#define	LAYOUT_NAME "layout_obj_debug"
#define	TYPE 0

struct root {
	PLIST_HEAD(listhead, struct tobj) lhead, lhead2;
};

struct tobj {
	PLIST_ENTRY(struct tobj) next;
};

void
test_FOREACH(const char *path)
{
	PMEMobjpool *pop = NULL;
	PMEMoid varoid, nvaroid;
	OID_TYPE(struct root) root;
	OID_TYPE(struct tobj) var, nvar;
	int type = 0;

#define	COMMANDS_FOREACH\
	do {\
	POBJ_FOREACH(pop, varoid, type) {}\
	POBJ_FOREACH_SAFE(pop, varoid, nvaroid, type) {}\
	POBJ_FOREACH_TYPE(pop, var, type) {}\
	POBJ_FOREACH_SAFE_TYPE(pop, var, nvar, type) {}\
	POBJ_LIST_FOREACH(var, &D_RW(root)->lhead, next) {}\
	POBJ_LIST_FOREACH_REVERSE(var, &D_RW(root)->lhead, next) {}\
	} while (0)

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	OID_ASSIGN(root, pmemobj_root(pop, sizeof (struct root)));
	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->lhead, 0, next);

	COMMANDS_FOREACH;
	TX_BEGIN(pop) {
		COMMANDS_FOREACH;
	} TX_END
	COMMANDS_FOREACH;

	pmemobj_close(pop);
}

void
test_lists(const char *path)
{
	PMEMobjpool *pop = NULL;
	OID_TYPE(struct root) root;
	OID_TYPE(struct tobj) elm;

#define	COMMANDS_LISTS\
	do {\
	POBJ_LIST_INSERT_NEW_HEAD(pop, &D_RW(root)->lhead, TYPE, next);\
	OID_ASSIGN(elm, pmemobj_alloc(pop, sizeof (struct tobj), TYPE));\
	POBJ_LIST_INSERT_AFTER(pop, &D_RW(root)->lhead,\
			POBJ_LIST_FIRST(&D_RW(root)->lhead), elm, next);\
	POBJ_LIST_MOVE_ELEMENT_HEAD(pop, &D_RW(root)->lhead,\
			&D_RW(root)->lhead2, elm, next, next);\
	POBJ_LIST_REMOVE(pop, &D_RW(root)->lhead2, elm, next);\
	pmemobj_free(elm.oid);\
	} while (0)

	if ((pop = pmemobj_create(path, LAYOUT_NAME,
					PMEMOBJ_MIN_POOL,
					S_IRWXU)) == NULL)
		FATAL("!pmemobj_create: %s", path);

	OID_ASSIGN(root, pmemobj_root(pop, sizeof (struct root)));

	COMMANDS_LISTS;
	TX_BEGIN(pop) {
		COMMANDS_LISTS;
	} TX_END
	COMMANDS_LISTS;

	pmemobj_close(pop);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_debug");

	if (argc != 3)
		FATAL("usage: %s file-name op:f|l", argv[0]);

	const char *path = argv[1];

	if (strchr("fl", argv[2][0]) == NULL || argv[2][1] != '\0')
		FATAL("op must be f or l");

	switch (argv[2][0]) {
		case 'f':
			test_FOREACH(path);
			break;
		case 'l':
			test_lists(path);
			break;
	}

	DONE(NULL);
}
