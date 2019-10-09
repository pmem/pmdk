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
 * config.c -- pmem2_config implementation
 */
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"

struct pmem2_config {
	int fd;
};

/*
 * pmem2_config_new -- allocates and initialize cfg structure.
 */
int
pmem2_config_new(struct pmem2_config **cfg)
{
	*cfg = Malloc(sizeof(**cfg));

	if (*cfg == NULL) {
		if (errno == ENOMEM) {
			ERR("pmem_config allocation failed");
			return PMEM2_E_NOMEM;
		} else {
			ERR("!malloc");
			return PMEM2_E_SYSERROR;
		}
	}

	(*cfg)->fd = -1;
	return 0;
}

/*
 * pmem2_config_set_fd -- sets fd in config struct
 */
int
pmem2_config_set_fd(struct pmem2_config *cfg, int fd)
{
	if (fd < 0) {
		cfg->fd = -1;
		return 0;
	}

	/* XXX: fdup()? an user can use F_SETFD to change flags */
	int flags = fcntl(fd, F_GETFL);

	if (flags == -1) {
		if (errno == EBADF) {
			ERR("fd is not open file descriptor");
			return PMEM2_E_INVAL;
		} else {
			ERR("!fcntl");
			return PMEM2_E_SYSERROR;
		}
	}

	if (flags & O_WRONLY) {
		ERR("fd must be open with O_RDONLY or O_WRONLY");
		return PMEM2_E_INVALID_HANDLE;
	}

	return 0;
}

/*
 * pmem2_config_delete -- dealocate cfg structure.
 */
int
pmem2_config_delete(struct pmem2_config **cfg)
{
	Free(*cfg);
	*cfg = NULL;
	return 0;
}
