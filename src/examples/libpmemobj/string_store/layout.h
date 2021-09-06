/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */

/*
 * layout.h -- example from introduction part 1
 */

#define LAYOUT_NAME "intro_1"
#define MAX_BUF_LEN 10

struct my_root {
	size_t len;
	char buf[MAX_BUF_LEN];
};
