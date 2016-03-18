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
 * util_cpuid.c -- unit test for CPU features detection
 */

#define	_GNU_SOURCE
#include "unittest.h"
#include "cpu.h"

#define	PROCMAXLEN 2048 /* maximum expected line length in /proc files */

/*
 * parse_cpuinfo -- parses one line from /proc/cpuinfo
 *
 * Returns 1 when line contains flags, 0 otherwise.
 */
static int
parse_cpuinfo(char *line)
{
	static const char flagspfx[] = "flags\t\t: ";
	static const char clflush[] = " clflush ";
	static const char clwb[] = " clwb ";
	static const char clflushopt[] = " clflushopt ";
	static const char pcommit[] = " pcommit ";
	static const char sse2[] = " sse2 ";

	if (strncmp(flagspfx, line, sizeof (flagspfx) - 1) != 0)
		return 0;

	/* start of list of flags */
	char *flags = &line[sizeof (flagspfx) - 2];

	/* change ending newline to space delimiter */
	char *nl = strrchr(line, '\n');
	if (nl)
		*nl = ' ';

	int clflush_present = strstr(flags, clflush) != NULL;
	UT_ASSERTeq(is_cpu_clflush_present(), clflush_present);

	int clflushopt_present = strstr(flags, clflushopt) != NULL;
	UT_ASSERTeq(is_cpu_clflushopt_present(), clflushopt_present);

	int clwb_present = strstr(flags, clwb) != NULL;
	UT_ASSERTeq(is_cpu_clwb_present(), clwb_present);

	int pcommit_present = strstr(flags, pcommit) != NULL;
	UT_ASSERTeq(is_cpu_pcommit_present(), pcommit_present);

	int sse2_present = strstr(flags, sse2) != NULL;
	UT_ASSERTeq(is_cpu_sse2_present(), sse2_present);

	return 1;
}

/*
 * check_cpuinfo -- validates CPU features detection
 */
static void
check_cpuinfo(void)
{
	/* detect supported CPU features */
	FILE *fp;
	if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
		UT_ERR("!/proc/cpuinfo");
	} else {
		char line[PROCMAXLEN];	/* for fgets() */

		while (fgets(line, PROCMAXLEN, fp) != NULL) {
			if (parse_cpuinfo(line))
				break;
		}

		fclose(fp);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_cpuid");

	check_cpuinfo();

	DONE(NULL);
}
