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
 * win_lists.c -- test list routines used in windows implementation
 */

#include "unittest.h"
#include <sys/queue.h>

typedef struct TEST_NODE {
	LIST_ENTRY(TEST_NODE) ListEntry;
	int dummy;
} *PTEST_NODE;

LIST_HEAD(TestList, TEST_NODE) TestListHead =
	LIST_HEAD_INITIALIZER(TestListHead);

void
dump_list()
{
	PTEST_NODE pNode = NULL;

	pNode = (PTEST_NODE)LIST_FIRST(&TestListHead);
	while (pNode != NULL) {
		UT_OUT("Node value: %d", pNode->dummy);
		pNode = (PTEST_NODE)LIST_NEXT(pNode, ListEntry);
	}
}

int
getListCount()
{
	PTEST_NODE pNode = NULL;
	int listCount = 0;

	pNode = (PTEST_NODE)LIST_FIRST(&TestListHead);
	while (pNode != NULL) {
		listCount++;
		pNode = (PTEST_NODE)LIST_NEXT(pNode, ListEntry);
	}
	return listCount;
}

PTEST_NODE
allocNode()
{
	PTEST_NODE pNode = NULL;
	pNode = malloc(sizeof(struct TEST_NODE));
	if (pNode == NULL)
		UT_FATAL("Unabe to allocate memory for test node");
	return pNode;
}


/*
 *  Do some basic list manipulations and output to log for
 *  script comparison. Only testing the macros we use.
 */
int
main(int argc, char *argv[])
{
	PTEST_NODE pNode = NULL;

	START(argc, argv, "win_lists");

	LIST_INIT(&TestListHead);
	UT_ASSERT_rt(LIST_EMPTY(&TestListHead));

	pNode = allocNode();
	pNode->dummy = 0;
	LIST_INSERT_HEAD(&TestListHead, pNode, ListEntry);
	UT_ASSERTeq_rt(1, getListCount());
	dump_list();

	/* Remove one node */
	LIST_REMOVE(pNode, ListEntry);
	UT_ASSERTeq_rt(0, getListCount());
	dump_list();
	free(pNode);

	/* Add a bunch of nodes */
	for (int i = 1; i < 10; i++) {
		pNode = allocNode();
		pNode->dummy = i;
		LIST_INSERT_HEAD(&TestListHead, pNode, ListEntry);
	}
	UT_ASSERTeq_rt(9, getListCount());
	dump_list();

	/* Remove all of them */
	while (!LIST_EMPTY(&TestListHead)) {
		PTEST_NODE pNode = (PTEST_NODE)LIST_FIRST(&TestListHead);
		LIST_REMOVE(pNode, ListEntry);
		free(pNode);
	}
	UT_ASSERTeq_rt(0, getListCount());
	dump_list();

	DONE(NULL);
}
