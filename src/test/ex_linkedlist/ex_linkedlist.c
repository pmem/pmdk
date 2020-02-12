// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * ex_linkedlist.c - test of linkedlist example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pmemobj_list.h"
#include "unittest.h"

#define ELEMENT_NO	10
#define PRINT_RES(res, struct_name) do {\
	if ((res) == 0) {\
		UT_OUT("Outcome for " #struct_name " is correct!");\
	} else {\
		UT_ERR("Outcome for " #struct_name\
				" does not match expected result!!!");\
	}\
} while (0)

POBJ_LAYOUT_BEGIN(list);
POBJ_LAYOUT_ROOT(list, struct base);
POBJ_LAYOUT_TOID(list, struct tqueuehead);
POBJ_LAYOUT_TOID(list, struct slisthead);
POBJ_LAYOUT_TOID(list, struct tqnode);
POBJ_LAYOUT_TOID(list, struct snode);
POBJ_LAYOUT_END(list);

POBJ_TAILQ_HEAD(tqueuehead, struct tqnode);
struct tqnode {
	int data;
	POBJ_TAILQ_ENTRY(struct tqnode) tnd;
};

POBJ_SLIST_HEAD(slisthead, struct snode);
struct snode {
	int data;
	POBJ_SLIST_ENTRY(struct snode) snd;
};

struct base {
	struct tqueuehead tqueue;
	struct slisthead slist;
};

static const int expectedResTQ[] = { 111, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 222 };
static const int expectedResSL[] = { 111, 8, 222, 6, 5, 4, 3, 2, 1, 0, 333 };

/*
 * dump_tq -- dumps list on standard output
 */
static void
dump_tq(struct tqueuehead *head, const char *str)
{
	TOID(struct tqnode) var;

	UT_OUT("%s start", str);
	POBJ_TAILQ_FOREACH(var, head, tnd)
		UT_OUT("%d", D_RW(var)->data);
	UT_OUT("%s end", str);
}

/*
 * init_tqueue -- initialize tail queue
 */
static void
init_tqueue(PMEMobjpool *pop, struct tqueuehead *head)
{
	if (!POBJ_TAILQ_EMPTY(head))
		return;

	TOID(struct tqnode) node;
	TOID(struct tqnode) middleNode;
	TOID(struct tqnode) node888;
	TOID(struct tqnode) tempNode;
	int i = 0;
	TX_BEGIN(pop) {
		POBJ_TAILQ_INIT(head);
		dump_tq(head, "after init");
		for (i = 0; i < ELEMENT_NO; ++i) {
			node = TX_NEW(struct tqnode);
			D_RW(node)->data = i;
			if (0 == i) {
				middleNode = node;
			}

			POBJ_TAILQ_INSERT_HEAD(head, node, tnd);
			node = TX_NEW(struct tqnode);
			D_RW(node)->data = i;
			POBJ_TAILQ_INSERT_TAIL(head, node, tnd);
		}
		dump_tq(head, "after insert[head|tail]");

		node = TX_NEW(struct tqnode);
		D_RW(node)->data = 666;
		POBJ_TAILQ_INSERT_AFTER(middleNode, node, tnd);
		dump_tq(head, "after insert_after1");

		middleNode = POBJ_TAILQ_NEXT(middleNode, tnd);

		node = TX_NEW(struct tqnode);
		D_RW(node)->data = 888;
		node888 = node;
		POBJ_TAILQ_INSERT_BEFORE(middleNode, node, tnd);
		dump_tq(head, "after insert_before1");
		node = TX_NEW(struct tqnode);
		D_RW(node)->data = 555;
		POBJ_TAILQ_INSERT_BEFORE(middleNode, node, tnd);
		dump_tq(head, "after insert_before2");

		node = TX_NEW(struct tqnode);
		D_RW(node)->data = 111;
		tempNode = POBJ_TAILQ_FIRST(head);
		POBJ_TAILQ_INSERT_BEFORE(tempNode, node, tnd);
		dump_tq(head, "after insert_before3");
		node = TX_NEW(struct tqnode);
		D_RW(node)->data = 222;
		tempNode = POBJ_TAILQ_LAST(head);
		POBJ_TAILQ_INSERT_AFTER(tempNode, node, tnd);
		dump_tq(head, "after insert_after2");

		tempNode = middleNode;
		middleNode = POBJ_TAILQ_PREV(tempNode, tnd);
		POBJ_TAILQ_MOVE_ELEMENT_TAIL(head, middleNode, tnd);
		dump_tq(head, "after move_element_tail");
		POBJ_TAILQ_MOVE_ELEMENT_HEAD(head, tempNode, tnd);
		dump_tq(head, "after move_element_head");

		tempNode = POBJ_TAILQ_FIRST(head);
		POBJ_TAILQ_REMOVE(head, tempNode, tnd);
		dump_tq(head, "after remove1");
		tempNode = POBJ_TAILQ_LAST(head);
		POBJ_TAILQ_REMOVE(head, tempNode, tnd);
		dump_tq(head, "after remove2");
		POBJ_TAILQ_REMOVE(head, node888, tnd);
		dump_tq(head, "after remove3");
	} TX_ONABORT {
		abort();
	} TX_END
}

/*
 * dump_sl -- dumps list on standard output
 */
static void
dump_sl(struct slisthead *head, const char *str)
{
	TOID(struct snode) var;

	UT_OUT("%s start", str);
	POBJ_SLIST_FOREACH(var, head, snd)
		UT_OUT("%d", D_RW(var)->data);
	UT_OUT("%s end", str);
}

/*
 * init_slist -- initialize SLIST
 */
static void
init_slist(PMEMobjpool *pop, struct slisthead *head)
{
	if (!POBJ_SLIST_EMPTY(head))
		return;

	TOID(struct snode) node;
	TOID(struct snode) tempNode;
	int i = 0;
	TX_BEGIN(pop) {
		POBJ_SLIST_INIT(head);
		dump_sl(head, "after init");

		for (i = 0; i < ELEMENT_NO; ++i) {
			node = TX_NEW(struct snode);
			D_RW(node)->data = i;
			POBJ_SLIST_INSERT_HEAD(head, node, snd);
		}
		dump_sl(head, "after insert_head");

		tempNode = POBJ_SLIST_FIRST(head);
		node = TX_NEW(struct snode);
		D_RW(node)->data = 111;
		POBJ_SLIST_INSERT_AFTER(tempNode, node, snd);
		dump_sl(head, "after insert_after1");

		tempNode = POBJ_SLIST_NEXT(node, snd);
		node = TX_NEW(struct snode);
		D_RW(node)->data = 222;
		POBJ_SLIST_INSERT_AFTER(tempNode, node, snd);
		dump_sl(head, "after insert_after2");

		tempNode = POBJ_SLIST_NEXT(node, snd);
		POBJ_SLIST_REMOVE_FREE(head, tempNode, snd);
		dump_sl(head, "after remove_free1");

		POBJ_SLIST_REMOVE_HEAD(head, snd);
		dump_sl(head, "after remove_head");

		TOID(struct snode) element = POBJ_SLIST_FIRST(head);
		while (!TOID_IS_NULL(D_RO(element)->snd.pe_next)) {
			element = D_RO(element)->snd.pe_next;
		}
		node = TX_NEW(struct snode);
		D_RW(node)->data = 333;
		POBJ_SLIST_INSERT_AFTER(element, node, snd);
		dump_sl(head, "after insert_after3");

		element = node;
		node = TX_NEW(struct snode);
		D_RW(node)->data = 123;
		POBJ_SLIST_INSERT_AFTER(element, node, snd);
		dump_sl(head, "after insert_after4");

		tempNode = POBJ_SLIST_NEXT(node, snd);
		POBJ_SLIST_REMOVE_FREE(head, node, snd);
		dump_sl(head, "after remove_free2");

	} TX_ONABORT {
		abort();
	} TX_END
}

int
main(int argc, char *argv[])
{
	unsigned res = 0;
	PMEMobjpool *pop;
	const char *path;

	START(argc, argv, "ex_linkedlist");

	/* root doesn't count */
	UT_COMPILE_ERROR_ON(POBJ_LAYOUT_TYPES_NUM(list) != 4);

	if (argc != 2) {
		UT_FATAL("usage: %s file-name", argv[0]);
	}
	path = argv[1];

	if (os_access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, POBJ_LAYOUT_NAME(list),
			PMEMOBJ_MIN_POOL, 0666)) == NULL) {
			UT_FATAL("!pmemobj_create: %s", path);
		}
	} else {
		if ((pop = pmemobj_open(path,
				POBJ_LAYOUT_NAME(list))) == NULL) {
			UT_FATAL("!pmemobj_open: %s", path);
		}
	}

	TOID(struct base) base = POBJ_ROOT(pop, struct base);
	struct tqueuehead *tqhead = &D_RW(base)->tqueue;
	struct slisthead *slhead = &D_RW(base)->slist;

	init_tqueue(pop, tqhead);
	init_slist(pop, slhead);

	int i = 0;
	TOID(struct tqnode) tqelement;
	POBJ_TAILQ_FOREACH(tqelement, tqhead, tnd) {
		if (D_RO(tqelement)->data != expectedResTQ[i]) {
			res = 1;
			break;
		}
		i++;
	}
	PRINT_RES(res, tail queue);

	i = 0;
	res = 0;
	TOID(struct snode) slelement;
	POBJ_SLIST_FOREACH(slelement, slhead, snd) {
		if (D_RO(slelement)->data != expectedResSL[i]) {
			res = 1;
			break;
		}
		i++;
	}
	PRINT_RES(res, singly linked list);
	pmemobj_close(pop);

	DONE(NULL);
}
