// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "core/membuf.h"

#define TEST_FUNC_DATA (void *)(0xDEADBEEF)
#define TEST_USER_DATA (void *)(0xC0FFEA)

#define TEST_ENTRY_PADDING (1 << 11)
#define MAX_TEST_ENTRIES (100000)

struct test_entry {
	enum membuf_check_result check;
	size_t size;
	char padding[TEST_ENTRY_PADDING];
};

static enum membuf_check_result
test_check(void *ptr, void *data)
{
	assert(data == TEST_FUNC_DATA);

	struct test_entry *entry = ptr;
	return entry->check;
}

static size_t
test_size(void *ptr, void *data)
{
	assert(data == TEST_FUNC_DATA);

	struct test_entry *entry = ptr;
	return entry->size;
}

static struct test_entry *
test_entry_new(struct membuf *mbuf, enum membuf_check_result check)
{
	struct test_entry *entry =
		membuf_alloc(mbuf, sizeof(struct test_entry));
	if (entry == NULL)
		return NULL;
	entry->check = check;
	entry->size = sizeof(struct test_entry);

	return entry;
}

int
main(int argc, char *argv[])
{
	struct membuf *mbuf = membuf_new(test_check, test_size, TEST_FUNC_DATA,
		TEST_USER_DATA);

	struct test_entry **entries =
		malloc(sizeof(struct test_entry *) * MAX_TEST_ENTRIES);
	assert(entries != NULL);

	int i;
	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			test_entry_new(mbuf, MEMBUF_PTR_IN_USE);
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);

		entries[i] = entry;
	}

	/* if this triggers, increase MAX_TEST_ENTRIES */
	assert(i != MAX_TEST_ENTRIES);

	int entries_max = i;

	for (i = 0; i < entries_max / 2; ++i) {
		struct test_entry *entry = entries[i];
		entry->check = MEMBUF_PTR_CAN_REUSE;
	}

	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			test_entry_new(mbuf, MEMBUF_PTR_IN_USE);
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);
	}
	assert(i == entries_max / 2);

	for (i = entries_max / 2; i < entries_max; ++i) {
		struct test_entry *entry = entries[i];
		entry->check = MEMBUF_PTR_CAN_REUSE;
	}

	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			test_entry_new(mbuf, MEMBUF_PTR_IN_USE);
		if (entry == NULL)
			break;
		assert(membuf_ptr_user_data(entry) == TEST_USER_DATA);
	}
	assert(i == entries_max / 2);

	membuf_delete(mbuf);
	free(entries);

	return 0;
}
