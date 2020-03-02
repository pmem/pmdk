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

#define REDO_LOG_SIZE 1000
#define MINIMUM_POOL_SIZE (sizeof(struct pool_hdr) + sizeof(struct node) * 100)
#define offset(pool, addr) ((uintptr_t)addr - (uintptr_t)pool)

static pmem2_persist_fn Persist;
static pmem2_flush_fn Flush;
static pmem2_drain_fn Drain;
static pmem2_memcpy_fn Memcpy;

struct redo_log_entry {
	uintptr_t offset;
	uint64_t data;
};

struct redo_log {
	uint64_t last;
	uint8_t apply;
	uint8_t unused[57]; /* align to cache line */
	struct redo_log_entry entries[REDO_LOG_SIZE];
};

struct node {
	uint64_t this;
	uint64_t prev;
	uint64_t next;
	uint64_t key;
	uint64_t value;
};

struct pool_hdr {
	struct redo_log redo;
	uint64_t free;
	uint64_t head;
	uint64_t pool_size;
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
		if (redo->apply || redo->last) {
			redo->last = 0;
			redo->apply = 0;
			Persist(&redo, 256); /* persist entire CL */
		}
		return; /* no redo log to process */
	}

	uint8_t *start = (uint8_t *)pool;
	for (int i = 0; i < redo->last; ++i) {
		uint64_t *node = (uint64_t *)&start[redo->entries[i].offset];
		*node = redo->entries[i].data;
		Flush(node, sizeof(*node));
	}

	Drain();
	redo->last = 0;
	redo->apply = 0;
	Persist(&redo, 256); /* persist entire CL */
}

/*
 * redo_add -- add an entry to redo log
 */
static void
redo_add(struct redo_log *redo, uintptr_t offset, uint64_t data)
{
	assert(redo->apply == 0);
	if (redo->last == REDO_LOG_SIZE) {
		fprintf(stderr, "redo log to small");
		exit(1);
	}

	struct redo_log_entry *entry = &redo->entries[redo->last++];

	entry->offset = (uintptr_t)offset;
	entry->data = data;
	/*
	 * To achive optimal performance we should flush only
	 * full cachelines if it is possible
	 */
	if (redo->last % 4 == 0)
		Flush(entry - 3, sizeof(*entry) * 4);
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
	int left = redo->last % 4;
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
static void
list_add(struct pool_layout *pool, uint64_t key, uint64_t value)
{
	struct node *node;
	struct node *next = NULL;
	struct node *prev = NULL;

	if (pool->hdr.pool_size < pool->hdr.free) {
		fprintf(stderr, "pool is full\n");
		exit(1);
	}

	node = &pool->nodes[pool->hdr.free];
	if (pool->hdr.free != 0) {
		next = &pool->nodes[pool->hdr.head];
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
		redo_add(redo, offset(pool, &node->next), UINT64_MAX);
	} else {
		redo_add(redo, offset(pool, &node->next), next->this);
		redo_add(redo, offset(pool, &next->prev), pool->hdr.free);
	}

	if (prev == NULL) {
		redo_add(redo, offset(pool, &node->prev), UINT64_MAX);
		redo_add(redo, offset(pool, &pool->hdr.head), pool->hdr.free);
	} else {
		redo_add(redo, offset(pool, &node->prev), prev->this);
		redo_add(redo, offset(pool, &prev->next), pool->hdr.free);
	}

	redo_add(redo, offset(pool, &node->key), key);
	redo_add(redo, offset(pool, &node->value), value);
	redo_add(redo, offset(pool, &node->this), pool->hdr.free);
	redo_add(redo, offset(pool, &pool->hdr.free), pool->hdr.free + 1);
	redo_commit(redo);
	redo_apply(pool);
}

/*
 * list_print -- dump content of a list
 */
static void
list_print(struct pool_layout *pool)
{
	if (pool->hdr.free == 0)
		return;

	struct node *node = &pool->nodes[pool->hdr.head];
	for (; node->next != UINT64_MAX; node = &pool->nodes[node->next]) {
		printf("%lu = %lu\n", node->key, node->value);
	}
	printf("%lu = %lu\n", node->key, node->value);
}

/*
 * list_check -- check consistency of a list
 */
static void
list_check(struct pool_layout *pool)
{
	if (pool->hdr.free == 0)
		return;

	char *c = malloc(pool->hdr.free);

	if (c == NULL) {
		perror("malloc");
		exit(1);
	}

	memset(c, 0, pool->hdr.free);

	struct node *node = &pool->nodes[pool->hdr.head];
	for (; node->next != UINT64_MAX; node = &pool->nodes[node->next]) {
		c[node->this] = 1;
	}
	c[node->this] = 1;

	for (int i = 0; i < pool->hdr.free; i++) {
		if (!c[i]) {
			fprintf(stderr, "consistency check failed: %d\n", i);
			exit(1);
		}
	}
}

/*
 * map_pool -- create pmem2_map for a given file descriptor
 */
static struct pmem2_map *
map_pool(int fd)
{
	struct pmem2_config *cfg;
	struct pmem2_map *map;
	struct pmem2_source *src;

	if (pmem2_config_new(&cfg)) {
		fprintf(stderr, "%s\n", pmem2_errormsg());
		exit(1);
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		exit(1);
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		fprintf(stderr, "%s\n", pmem2_errormsg());
		exit(1);
	}

	if (pmem2_map(cfg, src, &map)) {
		fprintf(stderr, "%s\n", pmem2_errormsg());
		exit(1);
	}

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

#define PRINT_ID(x) if (x != UINT64_MAX) printf("%lu", x); else printf("NULL")

static void
list_dump(struct pool_layout *pool)
{
	for (int i = 0; i < pool->hdr.free; i++) {
		struct node *node = &pool->nodes[i];
		PRINT_ID(node->prev);
		printf("<---%lu--->", node->this);
		PRINT_ID(node->next);
		printf("\t\t\tkey=%lu value=%lu\n", node->key, node->value);
	}
}

int
main(int argc, char *argv[])
{
	int fd;

	if (argc < 3) {
		print_help(argv[0]);
		exit(1);
	}

	const char *path = argv[1];
	const char *cmd = argv[2];
	uint64_t key = 0, value = 0;
	if (strcmp(cmd, "add") == 0) {
		if (argc != 5) {
			print_help(argv[0]);
			exit(1);
		}
		key = parse_uint64(argv[3]);
		value = parse_uint64(argv[4]);
	} else {
		if (argc != 3) {
			print_help(argv[0]);
			exit(1);
		}
	}

	if ((fd = open(path, O_RDWR)) < 0) {
		perror("open");
		exit(1);
	}

	struct pmem2_map *map = map_pool(fd);

	size_t size = pmem2_map_get_size(map);
	if (size < MINIMUM_POOL_SIZE) {
		fprintf(stderr, "pool size(%lu) smaller than minimum size(%lu)",
			size, MINIMUM_POOL_SIZE);
		exit(1);
	}

	Persist = pmem2_get_persist_fn(map);
	Flush = pmem2_get_flush_fn(map);
	Drain = pmem2_get_drain_fn(map);
	Memcpy = pmem2_get_memcpy_fn(map);

	struct pool_layout *pool = pmem2_map_get_address(map);

	redo_apply(pool);

	pool->hdr.pool_size =
		(size - sizeof(struct pool_hdr)) / sizeof(struct node);
	/* persist to suppress valgrind error */
	Persist(&pool->hdr.pool_size, sizeof(pool->hdr.pool_size));

	if (strcmp(cmd, "add") == 0) {
		list_add(pool, key, value);
	} else if (strcmp(cmd, "print") == 0) {
		list_print(pool);
	} else if (strcmp(cmd, "check") == 0) {
		list_check(pool);
	} else if (strcmp(cmd, "dump") == 0) {
		list_dump(pool);
	} else {
		print_help(argv[0]);
		exit(1);
	}
	pmem2_unmap(&map);
	close(fd);

	return 0;
}
