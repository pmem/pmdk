/*
 * Copyright 2015-2016, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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

typedef struct TEST_LIST_NODE {
	LIST_ENTRY(TEST_LIST_NODE) ListEntry;
	int dummy;
} *PTEST_LIST_NODE;

LIST_HEAD(TestList, TEST_LIST_NODE);

void
dump_list(struct TestList *head)
{
	PTEST_LIST_NODE pNode = NULL;

	pNode = (PTEST_LIST_NODE)LIST_FIRST(head);
	while (pNode != NULL) {
		UT_OUT("Node value: %d", pNode->dummy);
		pNode = (PTEST_LIST_NODE)LIST_NEXT(pNode, ListEntry);
	}
}

int
get_list_count(struct TestList *head)
{
	PTEST_LIST_NODE pNode = NULL;
	int listCount = 0;

	pNode = (PTEST_LIST_NODE)LIST_FIRST(head);
	while (pNode != NULL) {
		listCount++;
		pNode = (PTEST_LIST_NODE)LIST_NEXT(pNode, ListEntry);
	}
	return listCount;
}

/*
 * test_list - Do some basic list manipulations and output to log for
 * script comparison. Only testing the macros we use.
 */
void
test_list()
{
	PTEST_LIST_NODE pNode = NULL;
	struct TestList head = LIST_HEAD_INITIALIZER(head);

	LIST_INIT(&head);
	UT_ASSERT_rt(LIST_EMPTY(&head));

	pNode = MALLOC(sizeof(struct TEST_LIST_NODE));
	pNode->dummy = 0;
	LIST_INSERT_HEAD(&head, pNode, ListEntry);
	UT_ASSERTeq_rt(1, get_list_count(&head));
	dump_list(&head);

	/* Remove one node */
	LIST_REMOVE(pNode, ListEntry);
	UT_ASSERTeq_rt(0, get_list_count(&head));
	dump_list(&head);
	free(pNode);

	/* Add a bunch of nodes */
	for (int i = 1; i < 10; i++) {
		pNode = MALLOC(sizeof(struct TEST_LIST_NODE));
		pNode->dummy = i;
		LIST_INSERT_HEAD(&head, pNode, ListEntry);
	}
	UT_ASSERTeq_rt(9, get_list_count(&head));
	dump_list(&head);

	/* Remove all of them */
	while (!LIST_EMPTY(&head)) {
		PTEST_LIST_NODE pNode = (PTEST_LIST_NODE)LIST_FIRST(&head);
		LIST_REMOVE(pNode, ListEntry);
		free(pNode);
	}
	UT_ASSERTeq_rt(0, get_list_count(&head));
	dump_list(&head);
}

typedef struct TEST_SORTEDQ_NODE {
	SORTEDQ_ENTRY(TEST_SORTEDQ_NODE) queue_link;
	int dummy;
} TEST_SORTEDQ_NODE, *PTEST_SORTEDQ_NODE;

SORTEDQ_HEAD(TEST_SORTEDQ, TEST_SORTEDQ_NODE);

int
sortedq_node_comparer(TEST_SORTEDQ_NODE *a, TEST_SORTEDQ_NODE *b)
{
	return a->dummy - b->dummy;
}

struct TEST_DATA_SORTEDQ {
	int count;
	int data[10];
};

/*
 * test_sortedq - Do some basic operations on SORTEDQ and make sure that the
 * queue is sorted for different input sequences.
 */
void
test_sortedq()
{
	PTEST_SORTEDQ_NODE node = NULL;
	struct TEST_SORTEDQ head = SORTEDQ_HEAD_INITIALIZER(head);
	struct TEST_DATA_SORTEDQ test_data[] = {
		{5, {5, 7, 9, 100, 101}},
		{7, {1, 2, 3, 4, 5, 6, 7}},
		{5, {100, 90, 80, 70, 40}},
		{6, {10, 9, 8, 7, 6, 5}},
		{5, {23, 13, 27, 4, 15}},
		{5, {2, 2, 2, 2, 2}}
	};

	SORTEDQ_INIT(&head);
	UT_ASSERT_rt(SORTEDQ_EMPTY(&head));

	for (int i = 0; i < _countof(test_data); i++) {
		for (int j = 0; j < test_data[i].count; j++) {
			node = MALLOC(sizeof(TEST_SORTEDQ_NODE));
			node->dummy = test_data[i].data[j];
			SORTEDQ_INSERT(&head, node, queue_link,
				TEST_SORTEDQ_NODE, sortedq_node_comparer);
		}
		int prev = MININT;
		int num_entries = 0;
		SORTEDQ_FOREACH(node, &head, queue_link) {
			UT_ASSERT(prev <= node->dummy);
			num_entries++;
		}
		UT_ASSERT(num_entries == test_data[i].count);

		while (!SORTEDQ_EMPTY(&head)) {
			node = SORTEDQ_FIRST(&head);
			SORTEDQ_REMOVE(&head, node, queue_link);
			FREE(node);
		}
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "win_lists - testing %s",
		(argc > 1) ? argv[1] : "list");

	if (argc == 1 || (stricmp(argv[1], "list") == 0))
		test_list();
	if (argc > 1 && (stricmp(argv[1], "sortedq") == 0))
		test_sortedq();

	DONE(NULL);
}
