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
 * badblock_linux.c - implementation of the linux badblock query API
 */
#include "queue.h"
#include "file.h"
#include "os.h"
#include "out.h"
#include "sysfs.h"
#include "extent.h"
#include "badblock.h"
#include "plugin.h"
#ifndef _WIN32
#include "badblock_filesrc.h"
#include "badblock_poolset.h"
#endif

struct badblock_source {
	struct badblock_iter *(*iter_from_file)(const char *file);
	SLIST_ENTRY(badblock_source) e;
};

static SLIST_HEAD(, badblock_source) sources;

#ifndef _WIN32
/*
 * badblock_register_source -- registers a new badblock source
 */
static void
badblock_register_source(const char *name, void *funcs, void *arg)
{
	LOG(3, "%s", name);

	struct badblock_source *bsrc = Malloc(sizeof(*bsrc));
	if (bsrc == NULL) {
		ERR("failed to allocate badblock source");
		return;
	}

	bsrc->iter_from_file = funcs;

	SLIST_INSERT_HEAD(&sources, bsrc, e);
}
#endif

/*
 * badblock_new -- creates a new badblock iterator
 */
struct badblock_iter *
badblock_new(const char *path)
{
	struct badblock_iter *iter;

	if (SLIST_EMPTY(&sources)) {
#ifndef _WIN32
		badblock_poolset_source_register();
		badblock_file_source_register();
		plugin_load("badblock_source", 1,
			badblock_register_source, NULL);
#endif
	}

	struct badblock_source *bsrc;
	SLIST_FOREACH(bsrc, &sources, e) {
		if ((iter = bsrc->iter_from_file(path)) != NULL)
			break;
	}

	return iter;
}
