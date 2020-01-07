// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * uuid_windows.c -- pool set utilities with OS-specific implementation
 */

#include "uuid.h"
#include "out.h"

/*
 * util_uuid_generate -- generate a uuid
 */
int
util_uuid_generate(uuid_t uuid)
{
	HRESULT res = CoCreateGuid((GUID *)(uuid));
	if (res != S_OK) {
		ERR("CoCreateGuid");
		return -1;
	}
	return 0;
}
