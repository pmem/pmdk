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

static int loaded;

static int
foo(int a)
{
	return a;
}

static int
bar(int a)
{
	return a;
}

static struct dummy_plugin plugin_static_funcs = {
	foo, bar
};

static void
pmem_plugin_desc(const char **module_name, const char **name,
	unsigned *version, void **funcs)
{
	*module_name = "dummy";
	*name = "plugin_static";
	*version = 1;
	*funcs = &plugin_static_funcs;
}

static int
pmem_plugin_load(void)
{
	loaded = 1;

	return 0;
}

static void
pmem_plugin_unload(void)
{
	loaded = 2;
}

struct plugin_ops plugin_static = {
	pmem_plugin_desc, pmem_plugin_load, pmem_plugin_unload,
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
	} else if (strcmp(name, "plugin_static") == 0) {
		UT_ASSERTeq(p->foo(1), 1);
		UT_ASSERTeq(p->bar(1), 1);
	} else {
		UT_ASSERT(0);
	}

	nplugins++;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_plugin");

	int ret = plugin_init(".");
	UT_ASSERTeq(ret, 0);

	ret = plugin_add(&plugin_static);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(loaded, 0);

	plugin_load("dummy", 1, plugin_traverse, &nplugins);

	UT_ASSERTeq(loaded, 1);

	plugin_fini();

	UT_ASSERTeq(loaded, 2);

	UT_ASSERTeq(nplugins, 3);

	DONE(NULL);
}
