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
#include <errno.h>
#include "util.h"
#include "out.h"
#include "plugin.h"
#include "dlsym.h"

#define PLUGIN_DIR_DEFAULT "/usr/lib/pmem/plugins"
#define PLUGIN_DIR_ENV_VAR "PMEM_PLUGIN_DIR"

struct plugin {
	const char *module_name; /* identifier of the upper layer module */
	const char *name; /* unique identifier */
	unsigned version; /* struct version, bump on incompatibilities */
	void *funcs; /* the plugin itself */

	void *handle; /* shared library handle */

	int loaded; /* has the pmem_plugin_load function been called? */
	struct plugin_ops p_ops;

	SLIST_ENTRY(plugin) e;
};

static SLIST_HEAD(, plugin) plugins;

#define PMEM_PLUGIN_LOAD_SYMBOL(plugin, symbol, path, error)\
if (((plugin)->p_ops.symbol = util_dlsym((plugin)->handle, #symbol)) == NULL) {\
	LOG(3, "%s: unable to load %s symbol", (path), #symbol);\
	goto error; }

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

	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_desc,
		plugin_path, error_symbol);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_load,
		plugin_path, error_symbol);
	PMEM_PLUGIN_LOAD_SYMBOL(p, pmem_plugin_unload,
		plugin_path, error_symbol);

	p->p_ops.pmem_plugin_desc(&p->module_name,
		&p->name, &p->version, &p->funcs);

	SLIST_INSERT_HEAD(&plugins, p, e);

	return 0;

error_symbol:
	util_dlclose(p->handle);
error_plugin_open:
	Free(p);
error_plugin_alloc:
	return -1;
}

/*
 * plugin_init -- initializes plugin module
 */
int
plugin_init(const char *plugin_dir)
{
	LOG(3, "%s", plugin_dir);

	if (plugin_dir == NULL) {
		char *dir_env = getenv(PLUGIN_DIR_ENV_VAR);
		if (dir_env != NULL && dir_env[0] != '\0')
			plugin_dir = dir_env;
		else
			plugin_dir = PLUGIN_DIR_DEFAULT;
	}

	LOG(3, "loading plugins from %s", plugin_dir);

	struct dirent *entry;

	int olderrno = errno;
	DIR *pdir = opendir(plugin_dir);
	if (pdir == NULL) {
		errno = olderrno;
		return -1;
	}

	size_t dirlen = strlen(plugin_dir);

	char *plugin_path = Malloc(dirlen + PATH_MAX + 1);
	if (plugin_path == NULL) {
		errno = olderrno;
		closedir(pdir);
		return -1;
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

	errno = olderrno;
	return 0;
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

		if (p->handle != NULL)
			util_dlclose(p->handle);

		Free(p);
	}
}

/*
 * plugin_add -- add a static plugin
 */
int
plugin_add(const struct plugin_ops *p_ops)
{
	LOG(3, NULL);

	struct plugin *p = Malloc(sizeof(*p));
	if (p == NULL) {
		LOG(3, "unable to allocate static plugin");
		return -1;
	}

	p->loaded = 0;
	p->handle = NULL;
	p->p_ops = *p_ops;

	p->p_ops.pmem_plugin_desc(&p->module_name,
		&p->name, &p->version, &p->funcs);

	SLIST_INSERT_HEAD(&plugins, p, e);

	return 0;
}

/*
 * plugin_load -- traverses the plugins list, searching for compatible ones
 */
void
plugin_load(const char *module_name, unsigned version,
	void (*plugin_cb)(const char *name, void *funcs, void *arg), void *arg)
{
	LOG(3, "module_name %s version %u", module_name, version);

	struct plugin *p;
	SLIST_FOREACH(p, &plugins, e) {
		if (strcmp(module_name, p->module_name) != 0)
			continue;

		if (version != p->version)
			continue;

		if (!p->loaded) {
			if (p->p_ops.pmem_plugin_load() != 0) {
				ERR("unable to load %s plugin", p->name);
				continue;
			}
			p->loaded = 1;
			LOG(3, "loaded %s plugin from module %s",
				p->name, p->module_name);
		}

		plugin_cb(p->name, p->funcs, arg);
	}
}
