// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * layout.h -- example from introduction part 3
 */

#define MAX_BUF_LEN 10

POBJ_LAYOUT_BEGIN(string_store);
POBJ_LAYOUT_ROOT(string_store, struct my_root);
POBJ_LAYOUT_END(string_store);

struct my_root {
	char buf[MAX_BUF_LEN];
};
