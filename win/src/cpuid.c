/*
 * Copyright (c) 2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * cpuid.c -- CPU features detection
 */

#define	EAX_IDX 0
#define	EBX_IDX 1
#define	ECX_IDX 2
#define	EDX_IDX 3

/*
 * is_cpu_genuine_intel -- checks for genuine Intel CPU
 */
int
is_cpu_genuine_intel(void)
{
	int cpuinfo[4] = { 0 };

	union {
		char name[0x20];
		int cpuinfo[3];
	} vendor;

	memset(vendor.name, 0, sizeof (vendor.name));

	__cpuid(cpuinfo, 0);

	vendor.cpuinfo[0] = cpuinfo[EBX_IDX];
	vendor.cpuinfo[1] = cpuinfo[EDX_IDX];
	vendor.cpuinfo[2] = cpuinfo[ECX_IDX];

	return (strncmp(vendor.name, "GenuineIntel",
				sizeof (vendor.name))) == 0;
}

/*
 * is_cpu_sse2_present -- checks if SSE2 extensions are supported
 */
int
is_cpu_sse2_present()
{
	int cpuinfo[4] = { 0 };

	__cpuid(cpuinfo, 0x01);
	return (cpuinfo[EDX_IDX] & (1 << 26)) != 0;
}

/*
 * is_cpu_clflush_present -- checks if CLFLUSH instruction is supported
 */
int
is_cpu_clflush_present()
{
	int cpuinfo[4] = { 0 };

	__cpuid(cpuinfo, 0x01);
	return (cpuinfo[EDX_IDX] & (1 << 19)) != 0;
}

/*
 * is_cpu_clflushopt_present -- checks if CLFLUSHOPT instruction is supported
 */
int
is_cpu_clflushopt_present()
{
	int cpuinfo[4] = { 0 };

	if (!is_cpu_genuine_intel())
		return 0;
	__cpuid(cpuinfo, 0x07);
	return (cpuinfo[EBX_IDX] & (1 << 23)) != 0;
}

/*
 * is_cpu_pcommit_present -- checks if CLWB instruction is supported
 */
int
is_cpu_clwb_present()
{
	int cpuinfo[4] = { 0 };

	if (!is_cpu_genuine_intel())
		return 0;
	__cpuid(cpuinfo, 0x07);
	return (cpuinfo[EBX_IDX] & (1 << 24)) != 0;
}

/*
 * is_cpu_pcommit_present -- checks if PCOMMIT instruction is supported
 */
int
is_cpu_pcommit_present()
{
	int cpuinfo[4] = { 0 };

	if (!is_cpu_genuine_intel())
		return 0;
	__cpuid(cpuinfo, 0x07);
	return (cpuinfo[EBX_IDX] & (1 << 22)) != 0;
}
