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
 * plugin.c -- infrastructure for plugins
 */

#include <dirent.h>
#include <string.h>
#include <queue.h>
#include "util.h"
#include "out.h"
#include "plugin.h"
#include "dlsym.h"

struct plugin {
	const char *base_name;
	const char *name;
	unsigned version;

	void *handle;
	void *funcs;

	int loaded;

	struct {
		const char *(*pmem_plugin_base_name)(void);
		const char *(*pmem_plugin_name)(void);
		unsigned (*pmem_plugin_version)(void);
		int (*pmem_plugin_funcs)(void **funcs);

		int (*pmem_plugin_load)(void);
		void (*pmem_plugin_unload)(void);
	} p_ops;

	SLIST_ENTRY(plugin) e;
};

static SLIST_HEAD(, plugin) plugins;

#define PMEM_PLUGIN_LOAD_SYMBOL(plugin, symbol)\
(plugin)->p_ops.symbol = util_dlsym(p->handle, #symbol);\
if (p->p_ops.symbol == NULL) {\
	LOG(3, "%s: unable to load %s symbol", plugin_path, #symbol);\
	goto error_plugin_dlsym;\
}

/*
 * plugin_new_entry -- creates new plugin entry in the plugins list
 */
static int
plugin_new_entry(const char *plugin_path)
{
	LOG(3, "%s", plugin_path);

	struct plugin *p = Malloc(sizeof(*p));
	if (p == NULL) {
		LOG(3, "%s: unable to allocate plugin", plugin_path);
		goto error_plugin_alloc;
	}

	p->loaded = 0;

	p->handle = util_dlopen(plugin_path);
	if (p->handle == NULL) {
		LOG(3, "%s: unable to dlopen plugin (%s)",
			plugin_path, util_dlerror());
		goto error_plugin_open;
	}

	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_base_name);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_name);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_version);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_funcs);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_load);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_unload);

	p->base_name = p->p_ops.pmem_plugin_base_name();
	if (p->base_name == NULL) {
		LOG(4, "%s: invalid base name", plugin_path);

		goto error_plugin_dlsym;
	}
	p->name = p->p_ops.pmem_plugin_name();
	if (p->name == NULL) {
		LOG(4, "%s: invalid name", plugin_path);

		goto error_plugin_dlsym;
	}

	p->version = p->p_ops.pmem_plugin_version();
	if (p->version == 0) {
		LOG(4, "%s: invalid version", plugin_path);

		goto error_plugin_dlsym;
	}
	if (p->p_ops.pmem_plugin_funcs(&p->funcs) != 0) {
		LOG(4, "%s: unable to load plugin functionality", plugin_path);

		goto error_plugin_dlsym;
	}

	SLIST_INSERT_HEAD(&plugins, p, e);

	return 0;

error_plugin_dlsym:
	util_dlclose(p->handle);
error_plugin_open:
	Free(p);
error_plugin_alloc:
	return -1;
}

/*
 * plugin_init -- initializes plugin module
 */
void
plugin_init(const char *plugin_dir)
{
	LOG(3, "%s", plugin_dir);

	struct dirent *entry;

	DIR *pdir = opendir(plugin_dir);
	if (pdir == NULL)
		return;

	size_t dirlen = strlen(plugin_dir);

	char *plugin_path = Malloc(dirlen + PATH_MAX + 1);
	if (plugin_path == NULL) {
		closedir(pdir);
		return;
	}

	strcpy(plugin_path, plugin_dir);
	plugin_path[dirlen] = '/';

	while ((entry = readdir(pdir)) != NULL) {
		const char *extension = strrchr(entry->d_name, '.');
		if (extension == NULL || strcmp(extension, ".so") != 0)
			continue;

		strcpy(plugin_path + dirlen + 1, entry->d_name);

		plugin_new_entry(plugin_path);
	}

	closedir(pdir);
	Free(plugin_path);
}

/*
 * plugin_fini -- unloads all plugins and the module
 */
void
plugin_fini(void)
{
	LOG(3, NULL);

	struct plugin *p;
	while (!SLIST_EMPTY(&plugins)) {
		p = SLIST_FIRST(&plugins);

		SLIST_REMOVE_HEAD(&plugins, e);

		if (p->loaded)
			p->p_ops.pmem_plugin_unload();

		util_dlclose(p->handle);
		Free(p);
	}
}

void
plugin_load(const char *base_name, unsigned version,
	void (*plugin_cb)(const char *name, void *funcs, void *arg), void *arg)
{
	LOG(3, "base_name %s version %u", base_name, version);

	struct plugin *p;
	SLIST_FOREACH(p, &plugins, e) {
		if (strcmp(base_name, p->base_name) != 0)
			continue;

		if (version != p->version)
			continue;

		if (!p->loaded) {
			if (p->p_ops.pmem_plugin_load() != 0) {
				ERR("unable to load %s plugin", p->name);
				continue;
			}
		}

		plugin_cb(p->name, p->funcs, arg);
	}
}
