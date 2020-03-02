// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * basic.c -- simple example for the libpmem2
 */

#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>

#define CACHELINE 64
#define REDO_NENTRIES 1000
#define REDO_ENTRIES_IN_CL (CACHELINE / sizeof(struct redo_log_entry))
#define MINIMUM_POOL_SIZE (sizeof(struct pool_hdr) + sizeof(struct node) * 100)
#define offset(pool, addr) ((uintptr_t)(addr) - (uintptr_t)(pool))

static pmem2_persist_fn Persist;
static pmem2_flush_fn Flush;
static pmem2_drain_fn Drain;
static pmem2_memcpy_fn Memcpy;

struct redo_log_entry {
	uint64_t offset;
	uint64_t data;
};

struct redo_log {
	uint64_t last;
	uint8_t apply;
	uint8_t unused[CACHELINE - sizeof(uint64_t) - sizeof(uint8_t)];
	struct redo_log_entry entries[REDO_ENTRIES_IN_CL];
};

struct node {
	uint64_t id;
	uint64_t prev;
	uint64_t next;
	uint64_t key;
	uint64_t value;
};

struct pool_hdr {
	struct redo_log redo;
	uint64_t list_head;
	uint64_t pool_size;
	uint64_t list_free_node;
};

struct pool_layout {
	struct pool_hdr hdr;
	struct node nodes[];
};

/*
 * redo_apply -- process and apply the redo log
 */
static void
redo_apply(struct pool_layout *pool)
{
	struct redo_log *redo = &pool->hdr.redo;

	if (!redo->apply || !redo->last) {
		if (redo->apply || redo->last)
			goto exit;
		return; /* no redo log to process */
	}

	uint8_t *start = (uint8_t *)pool;
	for (uint64_t i = 0; i < redo->last; ++i) {
		uint64_t *node = (uint64_t *)&start + redo->entries[i].offset;
		*node = redo->entries[i].data;
		Flush(node, sizeof(*node));
	}

	Drain();
exit:	redo->last = 0;
	redo->apply = 0;
	Persist(&redo, CACHELINE); /* persist entire CL */
}

/*
 * redo_add -- add an entry to redo log
 */
static void
redo_add(struct redo_log *redo, uintptr_t offset, uint64_t data)
{
	assert(redo->apply == 0);
	assert(redo->last == REDO_ENTRIES_IN_CL);

	struct redo_log_entry *entry = &redo->entries[redo->last++];

	entry->offset = (uintptr_t)offset;
	entry->data = data;
	/*
	 * To achieve optimal performance we should flush only
	 * full cachelines if it is possible
	 */
	if (redo->last % REDO_ENTRIES_IN_CL == 0)
		Flush(entry - (REDO_ENTRIES_IN_CL - 1),
			sizeof(*entry) * REDO_ENTRIES_IN_CL);
}

/*
 * redo_commit -- commit redo log
 */
static void
redo_commit(struct redo_log *redo)
{
	if (redo->last == 0)
		return;

	/* flush last not full cacheline if needed */
	int left = redo->last % REDO_ENTRIES_IN_CL;
	if (left != 0)
		Flush(&redo->entries[redo->last] - (left - 1),
			sizeof(*redo->entries) * left);

	/* drain all previous flushes */
	Drain();
	redo->apply = 1;
	Persist(&redo->apply, sizeof(redo->last));
}

/*
 * list_add -- add a new node to the list
 */
static int
list_add(struct pool_layout *pool, uint64_t key, uint64_t value)
{
	struct node *node;
	struct node *next = NULL;
	struct node *prev = NULL;

	if (pool->hdr.pool_size < pool->hdr.list_free_node) {
		fprintf(stderr, "pool is full\n");
		return 1;
	}

	node = &pool->nodes[pool->hdr.list_free_node];
	if (pool->hdr.list_free_node != 0) {
		next = &pool->nodes[pool->hdr.list_head];
		while (next->key < key) {
			if (next->next == UINT64_MAX) {
				prev = next;
				next = NULL;
				break;
			}
			prev = next;
			next = &pool->nodes[next->next];
		}
	}
	struct redo_log *redo = &pool->hdr.redo;
	if (next == NULL) {
		node->next = UINT64_MAX;
	} else {
		node->next = next->id;
		redo_add(redo, offset(pool, &next->prev),
			pool->hdr.list_free_node);
	}

	if (prev == NULL) {
		node->prev = UINT64_MAX;
		redo_add(redo, offset(pool, &pool->hdr.list_head),
			pool->hdr.list_free_node);
	} else {
		node->prev = prev->id;
		redo_add(redo, offset(pool, &prev->next),
			pool->hdr.list_free_node);
	}

	node->key = key;
	node->value = value;
	node->id = pool->hdr.list_free_node;
	redo_add(redo, offset(pool, &pool->hdr.list_free_node),
		pool->hdr.list_free_node + 1);
	redo_commit(redo);
	redo_apply(pool);
	return 0;
}

/*
 * list_print -- dump content of a list
 */
static void
list_print(struct pool_layout *pool)
{
	if (pool->hdr.list_free_node == 0)
		return;

	struct node *node = &pool->nodes[pool->hdr.list_head];
	for (; node->next != UINT64_MAX; node = &pool->nodes[node->next]) {
		printf("%lu = %lu\n", node->key, node->value);
	}
	printf("%lu = %lu\n", node->key, node->value);
}

/*
 * list_check -- check consistency of a list
 */
static int
list_check(struct pool_layout *pool)
{
	if (pool->hdr.list_free_node == 0)
		return 0;

	char *c = malloc(pool->hdr.list_free_node);

	if (c == NULL) {
		perror("malloc");
		return 1;
	}

	memset(c, 0, pool->hdr.list_free_node);

	struct node *node = &pool->nodes[pool->hdr.list_head];
	for (; node->next != UINT64_MAX; node = &pool->nodes[node->next]) {
		c[node->id] = 1;
	}
	c[node->id] = 1;

	for (uint64_t i = 0; i < pool->hdr.list_free_node; i++) {
		if (!c[i]) {
			fprintf(stderr, "consistency check failed: %ld\n", i);
			return 1;
		}
	}
	return 0;
}

/*
 * map_pool -- create pmem2_map for a given file descriptor
 */
static struct pmem2_map *
map_pool(int fd)
{
	struct pmem2_config *cfg;
	struct pmem2_map *map = NULL;
	struct pmem2_source *src;

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		goto exit;
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		goto exit;
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto exit;
	}

	if (pmem2_map(cfg, src, &map)) {
		pmem2_perror("pmem2_map");
		goto exit;
	}
exit:
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	return map;
}

/*
 * parse_uint64 -- parse uint64_t string
 */
static uint64_t
parse_uint64(const char *str)
{
	char *end;
	errno = 0;
	uint64_t value = strtoull(str, &end, 0);
	if (errno == ERANGE || *end != '\0') {
		fprintf(stderr, "invalid argument %s\n", str);
		exit(1);
	}
	return value;
}

/*
 * print_help -- print help to the stderr
 */
static void
print_help(char *name)
{
	fprintf(stderr, "usage: %s file command add key value\n", name);
	fprintf(stderr, "       %s file command print\n", name);
	fprintf(stderr, "       %s file command check\n", name);
	fprintf(stderr, "       %s file command dump\n", name);
}

/*
 * print_id -- prints id of the the node
 */
static void inline
print_id(uint64_t id)
{
	if (id != UINT64_MAX)
		printf("%lu", id);
	else
		printf("NULL");
}

/*
 * list_dump -- dumps all allocated nodes
 */
static void
list_dump(struct pool_layout *pool)
{
	for (int i = 0; i < pool->hdr.list_free_node; i++) {
		struct node *node = &pool->nodes[i];
		print_id(node->prev);
		printf("<---%lu--->", node->id);
		print_id(node->next);
		printf("\t\t\tkey=%lu value=%lu\n", node->key, node->value);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	int ret = 0;
	if (argc < 3) {
		print_help(argv[0]);
		return 1;
	}

	const char *path = argv[1];
	const char *cmd = argv[2];
	uint64_t key = 0, value = 0;
	if (strcmp(cmd, "add") == 0) {
		if (argc != 5) {
			print_help(argv[0]);
			return 1;
		}
		key = parse_uint64(argv[3]);
		value = parse_uint64(argv[4]);
	} else {
		if (argc != 3) {
			print_help(argv[0]);
			return 1;
		}
	}

	if ((fd = open(path, O_RDWR)) < 0) {
		perror("open");
		return 1;
	}

	struct pmem2_map *map = map_pool(fd);

	size_t size = pmem2_map_get_size(map);
	if (size < MINIMUM_POOL_SIZE) {
		fprintf(stderr, "pool size(%lu) smaller than minimum size(%lu)",
			size, MINIMUM_POOL_SIZE);

		ret = 1;
		goto exit;
	}

	Persist = pmem2_get_persist_fn(map);
	Flush = pmem2_get_flush_fn(map);
	Drain = pmem2_get_drain_fn(map);
	Memcpy = pmem2_get_memcpy_fn(map);

	struct pool_layout *pool = pmem2_map_get_address(map);

	redo_apply(pool);

	pool->hdr.pool_size =
		(size - sizeof(struct pool_hdr)) / sizeof(struct node);

	Persist(&pool->hdr.pool_size, sizeof(pool->hdr.pool_size));

	if (strcmp(cmd, "add") == 0) {
		ret = list_add(pool, key, value);
	} else if (strcmp(cmd, "print") == 0) {
		list_print(pool);
	} else if (strcmp(cmd, "check") == 0) {
		ret = list_check(pool);
	} else if (strcmp(cmd, "dump") == 0) {
		list_dump(pool);
	} else {
		print_help(argv[0]);
		ret = 1;
	}
exit:
	pmem2_unmap(&map);
	close(fd);

	return ret;
}
