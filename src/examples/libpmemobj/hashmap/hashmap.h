// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */
#ifndef HASHMAP_H
#define HASHMAP_H

/* common API provided by both implementations */

#include <stddef.h>
#include <stdint.h>

struct hashmap_args {
	uint32_t seed;
};

enum hashmap_cmd {
	HASHMAP_CMD_REBUILD,
	HASHMAP_CMD_DEBUG,
};

#endif /* HASHMAP_H */
