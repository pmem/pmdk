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
 * pool_hdr_windows.c -- pool header utilities, Windows-specific
 */

#include <Shlwapi.h>
#include <elf.h>
#include "pool_hdr.h"
#include "out.h"

/*
 * arch_machine -- (internal) translate CPU arch into ELF-compatible machine id
 */
static int
arch_machine(WORD cpuarch)
{
	/* XXX: no support for other architectures yet */

	switch (cpuarch) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			return EM_X86_64;
		case PROCESSOR_ARCHITECTURE_IA64:
			return EM_IA_64;
		case PROCESSOR_ARCHITECTURE_INTEL:
			return EM_386;
		default:
			ASSERT(0); /* shouldn't happen */
			return EM_NONE;
	}
}

/*
 * arch_endianness -- (internal) determine endianness
 */
static int
arch_endianness(void)
{
	short word = (ELFDATA2MSB << 8) + ELFDATA2LSB;
	return ((char *)&word)[0];
}

/*
 * util_get_arch_flags -- get architecture identification flags
 */
int
util_get_arch_flags(struct arch_flags *arch_flags)
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	arch_flags->e_machine = arch_machine(si.wProcessorArchitecture);
#ifdef _WIN64
	arch_flags->ei_class = ELFCLASS64;
#else
	/*
	 * XXX - Just in case someone would remove the guard from platform.h
	 * and attempt to compile NVML for 32-bit.
	 */
	arch_flags->ei_class = ELFCLASS32;
#endif
	arch_flags->ei_data = arch_endianness();
	arch_flags->alignment_desc = alignment_desc();

	return 0;
}
