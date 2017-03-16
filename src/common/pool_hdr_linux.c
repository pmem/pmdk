/*
 * Copyright 2014-2017, Intel Corporation
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
 * pool_hdr_linux.c -- pool header utilities, Linux-specific
 */

#include <fcntl.h>
#include <link.h>
#include <string.h>
#include <unistd.h>

#include "out.h"
#include "os.h"
#include "pool_hdr.h"

/*
 * util_get_arch_flags -- get architecture identification flags
 */
int
util_get_arch_flags(struct arch_flags *arch_flags)
{
	char *path = "/proc/self/exe";
	int fd;
	ElfW(Ehdr) elf;
	int ret = 0;

	memset(arch_flags, 0, sizeof(*arch_flags));

	if ((fd = os_open(path, O_RDONLY)) < 0) {
		ERR("!open %s", path);
		ret = -1;
		goto out;
	}

	if (read(fd, &elf, sizeof(elf)) != sizeof(elf)) {
		ERR("!read %s", path);
		ret = -1;
		goto out_close;
	}

	if (elf.e_ident[EI_MAG0] != ELFMAG0 ||
	    elf.e_ident[EI_MAG1] != ELFMAG1 ||
	    elf.e_ident[EI_MAG2] != ELFMAG2 ||
	    elf.e_ident[EI_MAG3] != ELFMAG3) {
		ERR("invalid ELF magic");
		ret = -1;
		goto out_close;
	}

	arch_flags->e_machine = elf.e_machine;
	arch_flags->ei_class = elf.e_ident[EI_CLASS];
	arch_flags->ei_data = elf.e_ident[EI_DATA];
	arch_flags->alignment_desc = alignment_desc();

out_close:
	os_close(fd);
out:
	return ret;
}
