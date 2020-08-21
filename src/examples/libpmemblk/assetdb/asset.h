/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

#define ASSET_NAME_MAX 256
#define ASSET_USER_NAME_MAX 64
#define ASSET_CHECKED_OUT 2
#define ASSET_FREE 1

struct asset {
	char name[ASSET_NAME_MAX];
	char user[ASSET_USER_NAME_MAX];
	time_t time;
	int state;
};
