/*
 * Copyright 2019, Intel Corporation
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
 * config_linux.c -- linux specific pmem2_config implementation
 */
#include <errno.h>
#include <fcntl.h>
#include "config.h"
#include "os.h"
#include "out.h"
#include "pmem2_utils.h"

/*
 * pmem2_config_set_fd -- sets fd in config struct
 */
int
pmem2_config_set_fd(struct pmem2_config *cfg, int fd)
{
	if (fd < 0) {
		cfg->fd = INVALID_FD;
		return 0;
	}

	int flags = fcntl(fd, F_GETFL);

	if (flags == -1) {
		ERR("!fcntl");
		return PMEM2_E_ERRNO;
	}

	if ((flags & O_ACCMODE) == O_WRONLY) {
		ERR("fd must be open with O_RDONLY or O_RDRW");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	cfg->fd = fd;
	return 0;
}

int
pmem2_config_get_file_size(struct pmem2_config *cfg, size_t *size)
{
	LOG(3, "fd %d", cfg->fd);

	if (cfg->fd == INVALID_FD) {
		ERR("cannot check size for invalid file descriptor");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	os_stat_t st;

	if (os_fstat(cfg->fd, &st) < 0) {
		ERR("!fstat");
		return PMEM2_E_ERRNO;
	}

	enum pmem2_file_type type;
	int ret = pmem2_get_type_from_stat(&st, &type);
	if (ret)
		return ret;

	switch (type) {
		case PMEM2_FTYPE_DIR:
			ERR(
				"asking for size of a directory doesn't make any sense in context of pmem");
			return PMEM2_E_INVALID_FILE_HANDLE;
		case PMEM2_FTYPE_DEVDAX: {
			int ret = pmem2_device_dax_size_from_stat(&st, size);
			if (ret)
				return ret;
			break;
		}
		case PMEM2_FTYPE_REG:
			if (st.st_size < 0) {
				ERR(
					"kernel says size of regular file is negative (%ld)",
					st.st_size);
				return PMEM2_E_INVALID_FILE_HANDLE;
			}
			*size = (size_t)st.st_size;
			break;
		default:
			FATAL(
				"BUG: unhandled file type in pmem2_config_get_file_size");
	}

	LOG(4, "file length %zu", *size);
	return 0;
}
