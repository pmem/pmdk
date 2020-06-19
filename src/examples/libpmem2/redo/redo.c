// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * redo.c -- Implementation of simple persistent memory located redo log.
 *	This redo log is used to implement a doubly linked list.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#endif
#include <libpmem2.h>

#if defined(__x86_64) || defined(_M_X64) || defined(__aarch64__)
#define CACHELINE 64ULL
#elif defined(__PPC64__)
#define CACHELINE 128ULL
#else
#error unable to recognize architecture at compile time
#endif

#define REDO_NENTRIES 1000
#define LIST_ENTRY_NONE UINT64_MAX
#define REDO_ENTRIES_IN_CL (CACHELINE / sizeof(struct redo_log_entry))
#define POOL_SIZE_MIN (sizeof(struct pool_hdr) + sizeof(struct node) * 100)
#define offset(pool, addr) ((uintptr_t)(addr) - (uintptr_t)(&(pool)->hdr.redo))

static pmem2_persist_fn Persist;
static pmem2_flush_fn Flush;
static pmem2_drain_fn Drain;
static pmem2_memset_fn Memset;

struct pool_layout;

struct redo_log_entry {
	uint64_t offset;
	uint64_t data;
};

struct redo_log {
	struct redo_state {
		uint64_t last;
		uint8_t apply;
		uint8_t unused[CACHELINE - sizeof(uint64_t) - sizeof(uint8_t)];
	} state;
	struct redo_log_entry entries[REDO_ENTRIES_IN_CL];
};

/*
 * redo_apply -- process and apply the redo log
 */
static void
redo_apply(struct redo_log *redo)
{
	if (!redo->state.apply) {
		/*
		 * Redo log not committed.
		 * Just reset any potential leftovers.
		 */
		goto out;
	}

	uint8_t *start = (uint8_t *)redo;
	for (uint64_t i = 0; i < redo->state.last; ++i) {
		uint64_t *node = (uint64_t *)(start + redo->entries[i].offset);
		*node = redo->entries[i].data;
		Flush(node, sizeof(*node));
	}

	Drain();
out:
	/*
	 * reset 'apply' and 'last' fields - if memset will be interrupted
	 * it will be applied anyway as redo_apply() is called on each restart
	 */
	Memset(redo, 0, sizeof(struct redo_state), PMEM2_F_MEM_WC);
}

/*
 * redo_add -- add an entry to redo log
 */
static void
redo_add(struct redo_log *redo, uintptr_t offset, uint64_t data)
{
	assert(redo->state.apply == 0);
	assert(redo->state.last < REDO_ENTRIES_IN_CL);

	struct redo_log_entry *entry = &redo->entries[redo->state.last++];

	entry->offset = (uintptr_t)offset;
	entry->data = data;

	/* we will flush redo log at once when it's ready */
}

/*
 * redo_commit -- commit redo log
 */
static void
redo_commit(struct redo_log *redo)
{
	if (redo->state.last == 0)
		return;

	/* Persist entire redo log */
	Persist(&redo, sizeof(redo->state) +
		sizeof(*redo->entries) * redo->state.last);

	redo->state.apply = 1;
	Persist(&redo->state.apply, sizeof(redo->state.apply));
}

struct node {
	uint64_t id;
	uint64_t prev;
	uint64_t next;
	uint64_t key;
	uint64_t value;
};

struct pool_layout {
	struct pool_hdr {
		struct redo_log redo;
		uint64_t list_head;
		uint64_t list_nentries;
		uint64_t used_entries;
	} hdr;
	struct node nodes[];
};

/*
 * list_alloc_node -- alloc and initialize new node
 */
static struct node *
list_alloc_node(struct pool_layout *pool, uint64_t key, uint64_t value)
{
	struct node *node = &pool->nodes[pool->hdr.used_entries];
	/*
	 * Until used_entries is not updated allocated node is
	 * not persistent so we can update it without using redo log
	 */
	node->next = LIST_ENTRY_NONE;
	node->prev = LIST_ENTRY_NONE;
	node->key = key;
	node->value = value;
	node->id = pool->hdr.used_entries;

	redo_add(&pool->hdr.redo, offset(pool, &pool->hdr.used_entries),
		pool->hdr.used_entries + 1);

	return node;
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

	if (pool->hdr.list_nentries < pool->hdr.used_entries) {
		fprintf(stderr, "pool is full\n");
		return 1;
	}

	node = list_alloc_node(pool, key, value);

	/*
	 * Find the appropriate location where
	 * an allocated node is to be inserted.
	 */
	if (pool->hdr.used_entries != 0) {
		next = &pool->nodes[pool->hdr.list_head];
		while (next->key < key) {
			prev = next;
			if (next->next == LIST_ENTRY_NONE) {
				next = NULL;
				break;
			}
			next = &pool->nodes[next->next];
		}
	}

	struct redo_log *redo = &pool->hdr.redo;

	if (next != NULL) {
		node->next = next->id;
		redo_add(redo, offset(pool, &next->prev), node->id);
	}

	if (prev != NULL)
		node->prev = prev->id;

	uint64_t next_offset =
		offset(pool, prev ? &prev->next: &pool->hdr.list_head);

	redo_add(redo, next_offset, node->id);

	Flush(node, sizeof(*node));
	redo_commit(redo);
	redo_apply(redo);
	return 0;
}

/*
 * list_print -- dump content of a list
 */
static void
list_print(struct pool_layout *pool)
{
	if (pool->hdr.used_entries == 0)
		return;

	struct node *node = &pool->nodes[pool->hdr.list_head];
	printf("%" PRIu64 " = %" PRIu64 "\n", node->key, node->value);

	while (node->next != LIST_ENTRY_NONE) {
		node = &pool->nodes[node->next];
		printf("%" PRIu64 " = %" PRIu64 "\n", node->key, node->value);
	}
}

/*
 * print_id -- prints id of the the node
 */
static void inline
print_id(uint64_t id)
{
	if (id != LIST_ENTRY_NONE)
		printf("%" PRIu64, id);
	else
		printf("NULL");
}

/*
 * list_dump -- dumps all allocated nodes
 */
static void
list_dump(struct pool_layout *pool)
{
	printf("allocated entries: %" PRIu64 "\n", pool->hdr.used_entries);
	for (uint64_t i = 0; i < pool->hdr.used_entries; i++) {
		struct node *node = &pool->nodes[i];
		print_id(node->prev);
		printf("<---%" PRIu64 "--->", node->id);
		print_id(node->next);
		printf("\t\t\tkey=%" PRIu64 " value=%" PRIu64 "\n",
			node->key, node->value);
	}
}

/*
 * list_check -- check consistency of a list
 */
static int
list_check(struct pool_layout *pool)
{
	char *c = NULL;

	if (pool->hdr.used_entries == 0)
		return 0;

	if (pool->hdr.list_head >= pool->hdr.used_entries) {
		goto failed; /* first list entry is not allocated */
	}

	struct node *node = &pool->nodes[pool->hdr.list_head];

	if (node->prev != LIST_ENTRY_NONE) {
		goto failed; /* first list entry has previous node */
	}

	c = malloc(pool->hdr.used_entries);

	if (c == NULL) {
		perror("malloc");
		return 1;
	}

	memset(c, 0, pool->hdr.used_entries);

	for (; node->next != LIST_ENTRY_NONE &&
			node->next < pool->hdr.used_entries;
			node = &pool->nodes[node->next]) {
		c[node->id] = 1;
	}
	c[node->id] = 1;

	for (uint64_t i = 0; i < pool->hdr.used_entries; i++) {
		if (!c[i])
			goto failed; /* allocated node is not on the list  */
	}

	if (node->next != LIST_ENTRY_NONE)
		goto failed; /*  last list entry has next node */

	free(c);
	return 0;
failed:
	free(c);
	list_dump(pool);
	fprintf(stderr,
		"consistency check failed\n");
	return 1;
}

/*
 * pool_map -- create pmem2_map for a given file descriptor
 */
static struct pmem2_map *
pool_map(int fd, int map_private)
{
	struct pmem2_config *cfg;
	struct pmem2_map *map = NULL;
	struct pmem2_source *src;

	if (pmem2_config_new(&cfg)) {
		pmem2_perror("pmem2_config_new");
		goto err_cfg_new;
	}

	if (map_private && pmem2_config_set_sharing(cfg, PMEM2_PRIVATE)) {
		pmem2_perror("pmem2_config_set_sharing");
		goto err_cfg_set;
	}

	if (pmem2_config_set_required_store_granularity(cfg,
			PMEM2_GRANULARITY_PAGE)) {
		pmem2_perror("pmem2_config_set_required_store_granularity");
		goto err_cfg_set;
	}

	if (pmem2_source_from_fd(&src, fd)) {
		pmem2_perror("pmem2_source_from_fd");
		goto err_src_new;
	}

	if (pmem2_map(cfg, src, &map)) {
		pmem2_perror("pmem2_map");
		goto err_map;
	}

err_map:
	pmem2_source_delete(&src);
err_src_new:
err_cfg_set:
	pmem2_config_delete(&cfg);
err_cfg_new:
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
	fprintf(stderr, "usage: %s add pool key value\n", name);
	fprintf(stderr, "       %s print pool\n", name);
	fprintf(stderr, "       %s check pool\n", name);
	fprintf(stderr, "       %s dump pool\n", name);
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

	const char *path = argv[2];
	const char *cmd = argv[1];
	uint64_t key = 0, value = 0;
	int map_private = 0;

	if (strcmp(cmd, "add") == 0) {
		if ((argc - 3) % 2) {
			print_help(argv[0]);
			return 1;
		}
	} else {
		map_private = 1;
		if (argc != 3) {
			print_help(argv[0]);
			return 1;
		}
	}
	fd = open(path, O_RDWR);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	struct pmem2_map *map = pool_map(fd, map_private);

	if (map == NULL) {
		ret = 1;
		goto err_map;
	}

	size_t size = pmem2_map_get_size(map);
	if (size < POOL_SIZE_MIN) {
		fprintf(stderr,
			"pool size(%" PRIu64") smaller than minimum size(%"
			PRIu64 ")",
			size, POOL_SIZE_MIN);

		ret = 1;
		goto out;
	}

	Persist = pmem2_get_persist_fn(map);
	Flush = pmem2_get_flush_fn(map);
	Drain = pmem2_get_drain_fn(map);
	Memset = pmem2_get_memset_fn(map);

	struct pool_layout *pool = pmem2_map_get_address(map);

	redo_apply(&(pool->hdr.redo));
	pool->hdr.list_nentries =
		(size - sizeof(struct pool_hdr)) / sizeof(struct node);

	Persist(&pool->hdr.list_nentries, sizeof(pool->hdr.list_nentries));

	if (strcmp(cmd, "add") == 0) {
		for (int i = 3; i < argc; i += 2) {
			key = parse_uint64(argv[i]);
			value = parse_uint64(argv[i + 1]);
			ret = list_add(pool, key, value);
			if (ret)
				goto out;
		}
	} else if (strcmp(cmd, "print") == 0) {
		list_print(pool);
	} else if (strcmp(cmd, "check") == 0) {
		ret = list_check(pool);
	} else if (strcmp(cmd, "dump") == 0) {
		list_dump(pool);
	} else {
		fprintf(stderr, "invalid command %s\n", cmd);
		print_help(argv[0]);
		ret = 1;
	}
out:
	pmem2_unmap(&map);
err_map:
	close(fd);

	return ret;
}
