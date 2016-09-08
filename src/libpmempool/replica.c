/*
 * Copyright 2016, Intel Corporation
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
 * replica.c -- groups all command for replica manipulation
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "out.h"
#include "libpmemobj.h"
#include "lane.h"
#include "redo.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "libpmempool.h"
#include "set.h"
#include "file.h"

#include "sync.h"

/*
 * pmempool_sync -- copy one replica to another
 */
int
pmempool_sync(const char *poolset, struct pmempool_replica_opts *opts)
{
	int result;

	if (poolset == NULL) {
		ERR("poolset paths can not be NULL");
		errno = EINVAL;
		return -1;
	}

	/* check if poolset has correct signature */
	if (util_is_poolset(poolset) != 1) {
		ERR("!util_is_poolset");
		result = -1;
		goto err;
	}

	/* open poolset file */
	int fd_in = util_file_open(poolset, NULL, 0, O_RDONLY);
	if (fd_in < 0) {
		ERR("!util_file_open");
		result = -1;
		goto err;
	}

	/* fill up pool_set structure */
	struct pool_set *set_in = NULL;
	if (util_poolset_parse(poolset, fd_in, &set_in)) {
		ERR("Parsing input poolset failed");
		result = -1;
		goto err_close;
	}

	/* copy data from one replica to another */
	result = sync_replica(set_in, opts);

err_close:
	util_poolset_free(set_in);
	close(fd_in);
err:
	if (result != 0 && errno == 0)
		errno = EINVAL;
	return result;
}

/*
 * pmempool_poolset_convert -- alter poolset structure
 */
int
pmempool_transform(const char *poolset_in,
		const char *poolset_out, unsigned flags)
{
	int result;

	if (poolset_in == NULL || poolset_out == NULL) {
		ERR("poolset paths can not be NULL");
		errno = EINVAL;
		return -1;
	}

	if (util_is_poolset(poolset_in) != 1) {
		ERR("!util_is_poolset - input path");
		result = -1;
		goto err;
	}

	if (util_is_poolset(poolset_out) != 1) {
		ERR("!util_is_poolset - output path");
		result = -1;
		goto err;
	}

	int fd_in = util_file_open(poolset_in, NULL, 0, O_RDONLY);
	if (fd_in < 0) {
		ERR("!util_file_open - input path");
		result = -1;
		goto err;
	}

	int fd_out = util_file_open(poolset_out, NULL, 0, O_RDONLY);
	if (fd_out < 0) {
		ERR("!util_file_open - output path");
		result = -1;
		goto err_close_fin;
	}

	struct pool_set *set_in = NULL;
	struct pool_set *set_out = NULL;

	/* parse input poolset file */
	if (util_poolset_parse(poolset_in, fd_in, &set_in)) {
		ERR("!util_poolset_parse - input path");
		result = -1;
		goto err_close_finout;
	}

	/* parse output poolset file */
	if (util_poolset_parse(poolset_out, fd_out, &set_out)) {
		ERR("!util_poolset_parse - output path");
		result = -1;
		goto err_close_poolinfree;
	}

	ERR("Function not implemented");
	errno = ENOSYS;
	result = -1;

	util_poolset_free(set_out);
err_close_poolinfree:
	util_poolset_free(set_in);
err_close_finout:
	close(fd_out);
err_close_fin:
	close(fd_in);
err:
	if (result != 0 && errno == 0)
		errno = EINVAL;
	return result;
}
