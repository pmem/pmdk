// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * badblocks.c -- implementation of common bad blocks API
 */

#include "badblocks.h"
#include "alloc.h"
#include "out.h"
#include "log_internal.h"

/*
 * badblocks_new -- zalloc bad blocks structure
 */
struct badblocks *
badblocks_new(void)
{
	LOG(3, " ");

	struct badblocks *bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL) {
		ERR_W_ERRNO("Zalloc");
	}

	return bbs;
}

/*
 * badblocks_delete -- free bad blocks structure
 */
void
badblocks_delete(struct badblocks *bbs)
{
	LOG(3, "badblocks %p", bbs);

	if (bbs == NULL)
		return;

	Free(bbs->bbv);
	Free(bbs);
}
