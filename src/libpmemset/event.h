/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * event.h -- internal event definitions
 */
#ifndef PMEMSET_EVENT_H
#define PMEMSET_EVENT_H

#include <stddef.h>

/*
 * The synchronous event stream. These events are generated inside of the
 * flush/memcpy functions on the set.
 */
enum pmemset_event {
	PMEMSET_EVENT_COPY,
	PMEMSET_EVENT_FLUSH,
	PMEMSET_EVENT_DRAIN,
	PMEMSET_EVENT_PERSIST,
	PMEMSET_EVENT_BAD_BLOCK,
	PMEMSET_EVENT_PART_ADD,
	PMEMSET_EVENT_PART_REMOVE,
};

struct pmemset_event_copy {
	void *addr;
	size_t len;
};

struct pmemset_event_flush {
	void *addr;
	size_t len;
};

struct pmemset_event_drain {
	char stub;
};

struct pmemset_event_persist {
	void *addr;
	size_t len;
};

struct pmemset_event_bad_block {
	void *addr;
	size_t len;
};

struct pmemset_event_part_remove {
	void *addr;
	size_t len;
	int fd;
};

struct pmemset_event_part_add {
	void *addr;
	size_t len;
	int fd;
};

struct pmemset_event_context {
	enum pmemset_event type;
	union {
		char _data[64];
		struct pmemset_event_copy copy;
		struct pmemset_event_flush flush;
		struct pmemset_event_drain drain;
		struct pmemset_event_persist persist;
		struct pmemset_event_bad_block bad_block;
		struct pmemset_event_part_remove part_remove;
		struct pmemset_event_part_add part_add;
	} data;
};

#endif /* PMEMSET_EVENT_H */
