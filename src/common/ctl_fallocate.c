/*
 * Copyright 2018-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
	int *arg_out = arg;
	*arg_out = Fallocate_at_create;

	return 0;
}

static int
CTL_WRITE_HANDLER(at_create)(void *ctx, enum ctl_query_source source,
	void *arg, struct ctl_indexes *indexes)
{
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
