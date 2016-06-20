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
 * convert.c -- pmempool convert command source file
 */

#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdbool.h>
#include <sys/mman.h>
#include "common.h"
#include "convert.h"

static const char *help_str = "";

/*
 * print_usage -- print application usage short description
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s convert <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_convert_help -- print help message for convert command
 */
void
pmempool_convert_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

typedef int (*convert_func)(void *addr);

/*
 * Collection of pool converting functions. Each array index is used as a
 * source version.
 */
static convert_func version_convert[] = {
	NULL, /* from version 0 to version 1 - does not exist */
	convert_v1_v2, /* from v1 to v2 */
};

/*
 * pmempool_convert_func -- main function for convert command
 */
int
pmempool_convert_func(char *appname, int argc, char *argv[])
{
	if (argc != 2) {
		print_usage(appname);

		return -1;
	}

	int ret = 0;
	const char *f = argv[1];

	printf("This tool will update the pool to the latest available "
		"layout version.\nThis process is NOT fail-safe.\n"
		"Proceed only if the pool has been backed up or\n"
		"the risks are fully understood and acceptable.\n");
	if (ask_Yn('?', "convert the pool '%s' ?", f) != 'y') {
		return 0;
	}

	struct pool_set_file *psf = pool_set_file_open(f, 0, 0);

	if (psf == NULL) {
		perror(f);

		return -1;
	}
	void *addr = pool_set_file_map(psf, 0);
	if (addr == NULL) {
		perror(f);
		ret = -1;
		goto out;
	}

	struct pool_hdr *phdr = addr;
	if (memcmp(phdr->signature, OBJ_HDR_SIG, POOL_HDR_SIG_LEN) != 0) {
		fprintf(stderr, "Conversion is currently supported only for"
				"pmemobj pools\n");
		ret = -1;
		goto out;
	}

	uint32_t m = le32toh(phdr->major);
	if (m >= COUNT_OF(version_convert) || !version_convert[m]) {
		fprintf(stderr, "There's no conversion method for the pool.\n"
				"Please make sure the pmempool utility"
				"is up-to-date.\n");
		ret = -1;
		goto out;
	}

	PMEMobjpool *pop = addr;

	if (version_convert[m](pop) != 0)
		fprintf(stderr, "Failed to convert the pool\n");

	msync(pop, psf->size, 0);

out:
	pool_set_file_close(psf);

	return ret;
}
