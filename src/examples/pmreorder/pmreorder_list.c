// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2023, Intel Corporation */

/*
 * pmreorder_list.c -- explains how to use pmreorder tool with libpmem
 *
 * usage: pmreorder_list <g|b|c> <path>
 * g - good case - add element to the list in a consistent way
 * b - bad case - add element to the list in an inconsistent way
 * c - check persistent consistency of the list
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libpmem.h>

#define MAX_NODES 10
#define NODE_PTR(root, node_id) \
	(node_id == 0 ? NULL : &(root)->nodes[(node_id)])

typedef size_t node_id;

struct list_node {
	int value;
	node_id next;
};

struct list_root {
	node_id head;
	struct list_node nodes[MAX_NODES];
};

/*
 * check_consistency -- check if list meets the set requirements
 *
 * for consistent cases function returns 0
 * for inconsistent cases function returns 1
 */
static int
check_consistency(struct list_root *root)
{
	struct list_node *node = NODE_PTR(root, root->head);

	/*
	 * If node is linked to the list then its
	 * value should be set properly.
	 */
	if (node == NULL)
		return 0;

	do {
		if (node->value == 0)
			return 1;
		node = NODE_PTR(root, node->next);
	} while (node != NULL);

	return 0;
}

/*
 * list_print -- print all elements to the screen
 */
static void
list_print(struct list_root *list)
{
	FILE *fp;
	fp = fopen("pmreorder_list.log", "w+");
	if (fp == NULL) {
		perror("fopen pmreorder_list.log");
		exit(1);
	}

	fprintf(fp, "List:\n");

	struct list_node *node = NODE_PTR(list, list->head);
	if (node == NULL) {
		fprintf(fp, "List is empty");
		goto end;
	}

	do {
		fprintf(fp, "Value: %d\n", node->value);
		node = NODE_PTR(list, node->next);
	} while (node != NULL);

end:
	fclose(fp);
}

/*
 * list_insert_consistent -- add new element to the list in a consistent way
 */
static void
list_insert_consistent(struct list_root *root, node_id node, int value)
{
	struct list_node *new = NODE_PTR(root, node);
	if (new == NULL) {
		perror("cannot initialize a new node");
		exit(1);
	}

	new->value = value;
	new->next = root->head;
	pmem_persist(new, sizeof(*new));

	root->head = node;
	pmem_persist(&root->head, sizeof(root->head));
}

/*
 * list_insert_inconsistent -- add new element to the list
 * in an inconsistent way
 */
static void
list_insert_inconsistent(struct list_root *root, node_id node, int value)
{
	struct list_node *new = NODE_PTR(root, node);

	new->next = root->head;
	pmem_persist(&new->next, sizeof(node));

	root->head = node;
	pmem_persist(&root->head, sizeof(root->head));

	new->value = value;
	pmem_persist(&new->value, sizeof(value));
}

int
main(int argc, char *argv[])
{
	void *pmemaddr;
	size_t mapped_len;
	int is_pmem;
	int check;

	if (argc != 3 || strchr("cgb", argv[1][0]) == NULL ||
			argv[1][1] != '\0') {
		printf("Usage: pmreorder_list <c|g|b> <path>\n");
		exit(1);
	}

	/* create a pmem file and memory map it */
	pmemaddr = pmem_map_file(argv[2], 0, 0, 0, &mapped_len, &is_pmem);
	if (pmemaddr == NULL) {
		perror("pmem_map_file");
		exit(1);
	}
	struct list_root *r = pmemaddr;

	char opt = argv[1][0];
	if (strchr("gb", opt))
		pmem_memset_persist(r, 0, sizeof(struct list_root) +
				sizeof(struct list_node) * MAX_NODES);

	switch (opt) {
		case 'g':
			list_insert_consistent(r, 5, 55);
			list_insert_consistent(r, 3, 33);
			list_insert_consistent(r, 6, 66);
			break;
		case 'b':
			list_insert_inconsistent(r, 5, 55);
			list_insert_inconsistent(r, 3, 33);
			list_insert_inconsistent(r, 6, 66);
			break;
		case 'c':
			check = check_consistency(r);
			return check;
		default:
			printf("Unrecognized option: %c\n", opt);
			abort();
	}

	list_print(r);

	pmem_unmap(pmemaddr, mapped_len);
	return 0;
}
