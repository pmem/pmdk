/*
 * Copyright 2014-2016, Intel Corporation
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
 * util_windows.c -- general utilities with OS-specific implementation
 */

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>

#include <Shlwapi.h>

#include "util.h"
#include "out.h"

/*
 * util_map_hint -- determine hint address for mmap()
 *
 * XXX - no Windows implementation yet
 */
char *
util_map_hint(size_t len, size_t req_align)
{
	LOG(4, "hint not supported on windows");
	return NULL;
}

/*
 * util_tmpfile --  (internal) create the temporary file
 */
int
util_tmpfile(const char *dir, const char *templ)
{
	LOG(3, "dir \"%s\" template \"%s\"", dir, templ);

	int oerrno;
	int fd = -1;

	char *fullname = alloca(strlen(dir) + sizeof(templ));

	(void) strcpy(fullname, dir);
	(void) strcat(fullname, templ);

	/*
	 * XXX - block signals and modify file creation mask for the time
	 * of mkstmep() execution.  Restore previous settings once the file
	 * is created.
	 */

	fd = mkstemp(fullname);

	if (fd < 0) {
		ERR("!mkstemp");
		goto err;
	}

	(void) unlink(fullname);
	LOG(3, "unlinked file is \"%s\"", fullname);

	return fd;

err:
	oerrno = errno;
	if (fd != -1)
		(void) close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_get_arch_flags -- get architecture identification flags
 */
int
util_get_arch_flags(struct arch_flags *arch_flags)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	arch_flags->e_machine = si.wProcessorArchitecture;
	arch_flags->ei_class = 0; /* XXX - si.dwProcessorType */
	arch_flags->ei_data = 0;
	arch_flags->alignment_desc = alignment_desc();

	return 0;
}

/*
 * util_is_absolute_path -- check if the path is an absolute one
 */
int
util_is_absolute_path(const char *path)
{
	LOG(3, "path: %s", path);

	if (PathIsRelativeA(path))
		return 0;
	else
		return 1;
}
