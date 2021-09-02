// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2021, Intel Corporation */

/*
 * ctl_cow.c -- implementation of the CTL copy on write namespace
 */

#include "ctl.h"
#include "set.h"
#include "out.h"
#include "ctl_global.h"
#include "util.h"

/*
 * CTL_READ_HANDLER(at_open) -- returns at_open field
 */
static int
CTL_READ_HANDLER(at_open)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(ctx, source, indexes);

	int *arg_out = arg;
	*arg_out = COW_at_open;
	return 0;
}
/*
 * CTL_WRITE_HANDLER(at_open) -- sets the at_open field in copy_on_write
 */
static int
CTL_WRITE_HANDLER(at_open)(void *ctx,
	enum ctl_query_source source, void *arg, struct ctl_indexes *indexes)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(ctx, source, indexes);

	int arg_in = *(int *)arg;
	COW_at_open = arg_in;
	return 0;
}

static struct ctl_argument CTL_ARG(at_open) = CTL_ARG_BOOLEAN;

static const struct ctl_node CTL_NODE(copy_on_write)[] = {
	CTL_LEAF_RW(at_open),

	CTL_NODE_END
};

/*
 * cow_ctl_register -- registers ctl nodes for "copy_on_write" module
 */
void
ctl_cow_register(void)
{
	CTL_REGISTER_MODULE(NULL, copy_on_write);
}
