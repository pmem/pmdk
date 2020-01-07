// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * uuid_freebsd.c -- FreeBSD-specific implementation for UUID generation
 */

#include "uuid.h"

/* XXX Can't include <uuid/uuid.h> because it also defines uuid_t */
void uuid_generate(uuid_t);

/*
 * util_uuid_generate -- generate a uuid
 *
 * Uses the available FreeBSD uuid_generate library function.
 */
int
util_uuid_generate(uuid_t uuid)
{
	uuid_generate(uuid);

	return 0;
}
