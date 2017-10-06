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
 * badblock_test.c - implementation of badblock test source
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include "file.h"
#include "os.h"
#include "out.h"
#include "sysfs.h"
#include "extent.h"
#include "plugin.h"
#include "set.h"
#include "badblock_filesrc.h"

void pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs);
int pmem_plugin_load(void);
void pmem_plugin_unload(void);

struct badblock {
	const char *file;
	uint64_t offset;
	uint64_t length;
};

struct badblock_iter_test {
	struct {
		int (*next)(struct badblock_iter_test *iter,
			struct badblock *b);
		int (*clear)(struct badblock_iter_test *iter,
			struct badblock *b);
		void (*del)(struct badblock_iter_test *iter);
	} i_ops;

	char *file;
	char *file_config;
	int current_pos;
	struct sysfs_iter *badblock_iter;
};

/*
 * badblock_next -- returns the current badblock and bumps the iterator position
 */
static int
badblock_next(struct badblock_iter_test *iter, struct badblock *badblock)
{
	LOG(3, "iter %p badblock %p", iter, badblock);

	iter->current_pos++;

	badblock->file = iter->file;

	return sysfs_read(&iter->badblock_iter,
		iter->file_config, "%lu %lu",
		&badblock->offset, &badblock->length) == 2 ? 0 : -1;
}

/*
 * badblock_del -- deletes the badblock iterator instance
 */
static void
badblock_del(struct badblock_iter_test *iter)
{
	sysfs_early_delete(&iter->badblock_iter);
	Free(iter->file_config);
	Free(iter->file);
	Free(iter);
}

/*
 * badblock_clear -- clear a badblock
 */
static int
badblock_clear(struct badblock_iter_test *iter, struct badblock *b)
{
	LOG(3, "length %" PRIu64
		" offset %" PRIu64,
		b->length, b->offset);

	sysfs_early_delete(&iter->badblock_iter);
	struct sysfs_iter *s = NULL;

	char *tmp = Malloc(strlen(iter->file) + sizeof("_badblock_config_tmp"));
	ASSERTne(tmp, NULL);
	sprintf(tmp, "%s_badblock_config_tmp", iter->file);

	FILE *f = os_fopen(tmp, "w+");
	ASSERTne(f, NULL);

	int ret;
	struct badblock nb;
	while (sysfs_read(&s, iter->file_config, "%lu %lu",
		&nb.offset, &nb.length) == 2) {
		if (nb.offset == b->offset)
			continue;

		ret = fprintf(f, "%" PRIu64 " %" PRIu64 "\n",
			nb.offset, nb.length);
		ASSERTeq(ret, 3);
	}

	fclose(f);

	ret = os_unlink(iter->file_config);
	ASSERTeq(ret, 0);

	ret = rename(tmp, iter->file_config);
	ASSERTeq(ret, 0);

	iter->badblock_iter = NULL;
	iter->current_pos -= 1;

	int pos = 0;
	while (pos != iter->current_pos) {
		badblock_next(iter, &nb);
		pos++;
	}

	Free(tmp);

	return 0;
}

/*
 * iter_from_file -- creates a new badblock iterator
 */
static struct badblock_iter_test *
iter_from_file(const char *file)
{
	LOG(3, "%s", file);

	struct badblock_iter_test *iter = Malloc(sizeof(*iter));
	if (iter == NULL)
		return NULL;

	iter->current_pos = 0;
	iter->file = Strdup(file);
	if (file == NULL)
		goto error_file_alloc;

	iter->file_config = Malloc(strlen(file) + sizeof("_badblock_config"));
	ASSERTne(iter->file_config, NULL);
	sprintf(iter->file_config, "%s_badblock_config", iter->file);
	if (access(iter->file_config, F_OK) != 0)
		goto error_no_config;

	iter->badblock_iter = NULL;

	iter->i_ops.next = badblock_next;
	iter->i_ops.clear = badblock_clear;
	iter->i_ops.del = badblock_del;

	return iter;

error_no_config:
	Free(iter->file);
	Free(iter->file_config);
error_file_alloc:
	Free(iter);
	return NULL;
}

/*
 * pmem_plugin_desc -- linux file badblock descriptor
 */
void
pmem_plugin_desc(const char **module_name,
	const char **name, unsigned *version, void **funcs)
{
	*module_name = "badblock_source";
	*name = "test_badblock_source";
	*version = 1;
	*funcs = iter_from_file;
}

/*
 * pmem_plugin_load -- noop
 */
int
pmem_plugin_load(void)
{
	return 0;
}

/*
 * pmem_plugin_unload -- noop
 */
void
pmem_plugin_unload(void)
{

}
