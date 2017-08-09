/*
 * Copyright 2017, Intel Corporation
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
 * util_plugin.c -- unit test for plugin framework
 */

#include "unittest.h"
#include "util.h"
#include "out.h"
#include <inttypes.h>
#include "plugin.h"

struct dummy_plugin {
	int (*foo)(int a);
	int (*bar)(int a);
};

int nplugins;

static void
plugin_traverse(const char *name, void *funcs, void *arg)
{
	UT_ASSERTne(funcs, NULL);
	UT_ASSERTeq(arg, &nplugins);

	struct dummy_plugin *p = funcs;

	if (strcmp(name, "plugin0") == 0) {
		UT_ASSERTeq(p->foo(1), 4);
		UT_ASSERTeq(p->bar(1), 6);
	} else if (strcmp(name, "plugin1") == 0) {
		UT_ASSERTeq(p->foo(1), 6);
		UT_ASSERTeq(p->bar(1), 4);
	} else
		UT_ASSERT(0);

	nplugins++;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_plugin");

	plugin_init(".");

	plugin_load("dummy", 1, plugin_traverse, &nplugins);

	plugin_fini();

	UT_ASSERTeq(nplugins, 2);

	DONE(NULL);
}
