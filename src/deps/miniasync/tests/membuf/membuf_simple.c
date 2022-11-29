// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <stdio.h>
#include <stdlib.h>
#include "core/membuf.h"
#include "os_thread.h"
#include "test_helpers.h"

#define TEST_USER_DATA (void *)(0xC0FFEE)

#define TEST_ENTRY_PADDING (1 << 11)
#define MAX_TEST_ENTRIES (100000)

struct test_entry {
	char padding[TEST_ENTRY_PADDING];
};

void *
membuf_alloc_thread(void *arg)
{
	struct membuf *mbuf = arg;
	return membuf_alloc(mbuf, 1);
}

void
membuf_test_mt_reuse()
{
	struct membuf *mbuf = membuf_new(NULL);

	os_thread_t th1;
	os_thread_create(&th1, NULL, membuf_alloc_thread, mbuf);
	void *ptr1;
	os_thread_join(&th1, &ptr1);
	os_thread_t th2;
	os_thread_create(&th2, NULL, membuf_alloc_thread, mbuf);
	void *ptr2;
	os_thread_join(&th2, &ptr2);

	UT_ASSERTne(ptr1, ptr2);

	membuf_delete(mbuf);
}

void
membuf_test_st_reuse()
{
	struct membuf *mbuf = membuf_new(TEST_USER_DATA);
	UT_ASSERTne(mbuf, NULL);

	struct test_entry **entries =
		malloc(sizeof(struct test_entry *) * MAX_TEST_ENTRIES);
	UT_ASSERTne(entries, NULL);

	int i;
	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		UT_ASSERTeq(membuf_ptr_user_data(entry), TEST_USER_DATA);

		entries[i] = entry;
	}

	/* if this triggers, increase MAX_TEST_ENTRIES */
	UT_ASSERTne(i, MAX_TEST_ENTRIES);

	int entries_max = i;

	for (i = 0; i < entries_max / 2; ++i) {
		struct test_entry *entry = entries[i];
		membuf_free(entry);
	}

	int allocated = 0;
	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		UT_ASSERTeq(membuf_ptr_user_data(entry), TEST_USER_DATA);
	}
	allocated += i;
	UT_ASSERTeq(i, entries_max / 2);

	for (i = entries_max / 2; i < entries_max; ++i) {
		struct test_entry *entry = entries[i];
		membuf_free(entry);
	}

	for (i = 0; i < MAX_TEST_ENTRIES; ++i) {
		struct test_entry *entry =
			membuf_alloc(mbuf, sizeof(struct test_entry));
		if (entry == NULL)
			break;
		UT_ASSERTeq(membuf_ptr_user_data(entry), TEST_USER_DATA);
	}
	allocated += i;
	UT_ASSERTeq(allocated, entries_max);

	membuf_delete(mbuf);
	free(entries);
}

int
main(int argc, char *argv[])
{
	membuf_test_st_reuse();
	membuf_test_mt_reuse();

	return 0;
}
