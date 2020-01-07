// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * queue.c -- array based queue example
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <libpmemobj.h>

POBJ_LAYOUT_BEGIN(queue);
POBJ_LAYOUT_ROOT(queue, struct root);
POBJ_LAYOUT_TOID(queue, struct entry);
POBJ_LAYOUT_TOID(queue, struct queue);
POBJ_LAYOUT_END(queue);

struct entry { /* queue entry that contains arbitrary data */
	size_t len; /* length of the data buffer */
	char data[];
};

struct queue { /* array-based queue container */
	size_t front; /* position of the first entry */
	size_t back; /* position of the last entry */

	size_t capacity; /* size of the entries array */
	TOID(struct entry) entries[];
};

struct root {
	TOID(struct queue) queue;
};

/*
 * queue_constructor -- constructor of the queue container
 */
static int
queue_constructor(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct queue *q = ptr;
	size_t *capacity = arg;
	q->front = 0;
	q->back = 0;
	q->capacity = *capacity;

	/* atomic API requires that objects are persisted in the constructor */
	pmemobj_persist(pop, q, sizeof(*q));

	return 0;
}

/*
 * queue_new -- allocates a new queue container using the atomic API
 */
static int
queue_new(PMEMobjpool *pop, TOID(struct queue) *q, size_t nentries)
{
	return POBJ_ALLOC(pop,
		q,
		struct queue,
		sizeof(struct queue) + sizeof(TOID(struct entry)) * nentries,
		queue_constructor,
		&nentries);
}

/*
 * queue_nentries -- returns the number of entries
 */
static size_t
queue_nentries(struct queue *queue)
{
	return queue->back - queue->front;
}

/*
 * queue_enqueue -- allocates and inserts a new entry into the queue
 */
static int
queue_enqueue(PMEMobjpool *pop, struct queue *queue,
	const char *data, size_t len)
{
	if (queue->capacity - queue_nentries(queue) == 0)
		return -1; /* at capacity */

	/* back is never decreased, need to calculate the real position */
	size_t pos = queue->back % queue->capacity;

	int ret = 0;

	printf("inserting %zu: %s\n", pos, data);

	TX_BEGIN(pop) {
		/* let's first reserve the space at the end of the queue */
		TX_ADD_DIRECT(&queue->back);
		queue->back += 1;

		/* now we can safely allocate and initialize the new entry */
		TOID(struct entry) entry = TX_ALLOC(struct entry,
			sizeof(struct entry) + len);
		D_RW(entry)->len = len;
		memcpy(D_RW(entry)->data, data, len);

		/* and then snapshot the queue entry that we will modify */
		TX_ADD_DIRECT(&queue->entries[pos]);
		queue->entries[pos] = entry;
	} TX_ONABORT { /* don't forget about error handling! ;) */
		ret = -1;
	} TX_END

	return ret;
}

/*
 * queue_dequeue - removes and frees the first element from the queue
 */
static int
queue_dequeue(PMEMobjpool *pop, struct queue *queue)
{
	if (queue_nentries(queue) == 0)
		return -1; /* no entries to remove */

	/* front is also never decreased */
	size_t pos = queue->front % queue->capacity;

	int ret = 0;

	printf("removing %zu: %s\n", pos, D_RO(queue->entries[pos])->data);

	TX_BEGIN(pop) {
		/* move the queue forward */
		TX_ADD_DIRECT(&queue->front);
		queue->front += 1;
		/* and since this entry is now unreachable, free it */
		TX_FREE(queue->entries[pos]);
		/* notice that we do not change the PMEMoid itself */
	} TX_ONABORT {
		ret = -1;
	} TX_END

	return ret;
}

/*
 * queue_show -- prints all queue entries
 */
static void
queue_show(PMEMobjpool *pop, struct queue *queue)
{
	size_t nentries = queue_nentries(queue);
	printf("Entries %zu/%zu\n", nentries, queue->capacity);
	for (size_t i = queue->front; i < queue->back; ++i) {
		size_t pos = i % queue->capacity;
		printf("%zu: %s\n", pos, D_RO(queue->entries[pos])->data);
	}
}

/* available queue operations */
enum queue_op {
	UNKNOWN_QUEUE_OP,
	QUEUE_NEW,
	QUEUE_ENQUEUE,
	QUEUE_DEQUEUE,
	QUEUE_SHOW,

	MAX_QUEUE_OP,
};

/* queue operations strings */
static const char *ops_str[MAX_QUEUE_OP] =
	{"", "new", "enqueue", "dequeue", "show"};

/*
 * parse_queue_op -- parses the operation string and returns matching queue_op
 */
static enum queue_op
queue_op_parse(const char *str)
{
	for (int i = 0; i < MAX_QUEUE_OP; ++i)
		if (strcmp(str, ops_str[i]) == 0)
			return (enum queue_op)i;

	return UNKNOWN_QUEUE_OP;
}

/*
 * fail -- helper function to exit the application in the event of an error
 */
static void __attribute__((noreturn)) /* this function terminates */
fail(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	enum queue_op op;
	if (argc < 3 || (op = queue_op_parse(argv[2])) == UNKNOWN_QUEUE_OP)
		fail("usage: file-name [new <n>|show|enqueue <data>|dequeue]");

	PMEMobjpool *pop = pmemobj_open(argv[1], POBJ_LAYOUT_NAME(queue));
	if (pop == NULL)
		fail("failed to open the pool");

	TOID(struct root) root = POBJ_ROOT(pop, struct root);
	struct root *rootp = D_RW(root);
	size_t capacity;

	switch (op) {
		case QUEUE_NEW:
			if (argc != 4)
				fail("missing size of the queue");

			char *end;
			errno = 0;
			capacity = strtoull(argv[3], &end, 0);
			if (errno == ERANGE || *end != '\0')
				fail("invalid size of the queue");

			if (queue_new(pop, &rootp->queue, capacity) != 0)
				fail("failed to create a new queue");
		break;
		case QUEUE_ENQUEUE:
			if (argc != 4)
				fail("missing new entry data");

			if (D_RW(rootp->queue) == NULL)
				fail("queue must exist");

			if (queue_enqueue(pop, D_RW(rootp->queue),
				argv[3], strlen(argv[3]) + 1) != 0)
				fail("failed to insert new entry");
		break;
		case QUEUE_DEQUEUE:
			if (D_RW(rootp->queue) == NULL)
				fail("queue must exist");

			if (queue_dequeue(pop, D_RW(rootp->queue)) != 0)
				fail("failed to remove entry");
		break;
		case QUEUE_SHOW:
			if (D_RW(rootp->queue) == NULL)
				fail("queue must exist");

			queue_show(pop, D_RW(rootp->queue));
		break;
		default:
			assert(0); /* unreachable */
		break;
	}

	pmemobj_close(pop);

	return 0;
}
