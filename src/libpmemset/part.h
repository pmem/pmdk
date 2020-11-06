/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * part.h -- internal definitions for libpmemset part API
 */
#ifndef PMEMSET_PART_H
#define PMEMSET_PART_H

#include <stddef.h>

struct pmemset_part {
	char stub;
};

struct pmemset_part_map {
	void *addr;
	size_t length;
	char stub;
};

/*
 * Shutdown state data must be stored by the user externally for reliability.
 * This needs to be read by the user and given to the add part function so that
 * the current shutdown state can be compared with the old one.
 */
struct pmemset_part_shutdown_state_data {
	const char data[1024];
};

#endif /* PMEMSET_PART_H */
