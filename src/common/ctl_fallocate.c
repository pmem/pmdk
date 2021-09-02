// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2021, Intel Corporation */

/*
 * ctl_fallocate.c -- implementation of the fallocate CTL namespace
 */

#include "ctl.h"
#include "set.h"
#include "out.h"
#include "ctl_global.h"
#include "file.h"

static int
CTL_READ_HANDLER(at_create)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(ctx, source, indexes);

	int *arg_out = arg;
	*arg_out = Fallocate_at_create;

	return 0;
}

static int
CTL_WRITE_HANDLER(at_create)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(ctx, source, indexes);

	int arg_in = *(int *)arg;
	Fallocate_at_create = arg_in;

	return 0;
}

static struct ctl_argument CTL_ARG(at_create) = CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(fallocate)[] = {
	CTL_LEAF_RW(at_create),

	CTL_NODE_END
};

void
ctl_fallocate_register(void)
{
	CTL_REGISTER_MODULE(NULL, fallocate);
}
