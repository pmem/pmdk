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
 * rpmemd.c -- rpmemd main source file
 */

#include <stdio.h>
#include <getopt.h>

#define	DAEMON_NAME	"rpmemd"

/*
 * long_options -- command line arguments
 */
static const struct option long_options[] = {
	{"version",	no_argument,	0,	'V'},
	{"help",	no_argument,	0,	'h'},
	{0,		0,		0,	 0 },
};

/*
 * print_version -- (internal) prints version message
 */
static void
print_version(void)
{
	printf("%s version %s\n", DAEMON_NAME, SRCVERSION);
}

/*
 * print_usage -- (internal) prints usage message
 */
static void
print_usage(void)
{
	printf("usage: %s [--version] [--help] [<args>]\n", DAEMON_NAME);
}

/*
 * print_help -- (internal) prints help message
 */
static void
print_help(void)
{
	print_usage();
	print_version();
	printf("\n");
	printf("Options:\n");
	printf("  -V, --version        display version\n");
	printf("  -h, --help           display this help and exit\n");
	printf("\n");
	printf("For complete documentation see %s(1) manual page.\n",
			DAEMON_NAME);
}

int
main(int argc, char *argv[])
{
	int opt;
	int option_index;

	while ((opt = getopt_long(2, argv, "Vh",
			long_options, &option_index)) != -1) {
		switch (opt) {
		case 'V':
			print_version();
			return 0;
		case 'h':
			print_help();
			return 0;
		default:
			print_usage();
			return -1;
		}
	}

	return 0;
}
