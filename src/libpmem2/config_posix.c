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
#include <stdbool.h>
#include <unistd.h>

#include "out.h"
#include "config.h"

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
		if (errno == EBADF) {
			ERR("fd is not open file descriptor");
			return PMEM2_E_INVALID_ARG;
		} else {
			ERR("!fcntl");
			return PMEM2_E_EXTERNAL;
		}
	}

	if ((flags & O_ACCMODE) == O_WRONLY) {
		ERR("fd must be open with O_RDONLY or O_RDRW");
		return PMEM2_E_INVALID_HANDLE;
	}

	cfg->fd = fd;
	return 0;
}

/*
 * pmem2_config_fd_dup -- duplicate the file descriptor from src to dst
 */
int
pmem2_config_fd_dup(struct pmem2_config *dst, const struct pmem2_config *src)
{
	/* the destination fd has to be invalid */
	ASSERTeq(dst->fd, INVALID_FD);

	/* do not duplicate an invalid file descriptor */
	if (src->fd == INVALID_FD) {
		dst->fd = INVALID_FD;
		return PMEM2_E_OK;
	}

	int newfd = dup(src->fd);

	if (newfd == -1) {
		ERR("!dup");
		return PMEM2_E_EXTERNAL;
	}

	dst->fd = newfd;
	dst->user_owned_fd = false;

	return PMEM2_E_OK;
}

/*
 * pmem2_config_fd_close - close the duplicated file descriptor
 * For the user-owned file descriptor, it is NOP.
 */
int
pmem2_config_fd_close(struct pmem2_config *cfg)
{
	if (cfg->user_owned_fd)
		return PMEM2_E_OK;

	if (cfg->fd == INVALID_FD)
		return PMEM2_E_OK;

	if (close(cfg->fd)) {
		ERR("!close");
		return PMEM2_E_EXTERNAL;
	}

	cfg->fd = INVALID_FD;

	return PMEM2_E_OK;
}
