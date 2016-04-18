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

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <unistd.h>

#include "rpmemd.h"
#include "rpmemd_options.h"
#include "rpmemd_log.h"

static const char *optstr = "Vhvfo:l:";

/* non-printable value because this will be just a long option */
#define	OPT_SYSLOG	0xFF00

/*
 * long_options -- command line arguments
 */
static const struct option long_options[] = {
	{"version",	no_argument,		0,	'V'},
	{"help",	no_argument,		0,	'h'},
	{"verbose",	no_argument,		0,	'v'},
	{"foreground",	no_argument,		0,	'f'},
	{"log-file",	required_argument,	0,	'o'},
	{"log-level",	required_argument,	0,	'l'},
	{"use-syslog",	no_argument,		0,	OPT_SYSLOG},
	{0,		0,			0,	 0 },
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

static const char *help_str = "\
\n\
Options:\n\
  -V, --version            display version\n\
  -h, --help               display this help and exit\n\
  -v, --verbose            increase verbosity level\n\
  -f, --foreground         run daemon in foreground\n\
  -o, --log-file  <path>   use specified file instead of syslog\n\
  -l, --log-level <level>  set log level value\n\
      --use-syslog         use syslog(3) for logging messages\n\
\n\
For complete documentation see %s(1) manual page.\n\
";

/*
 * print_help -- prints help message
 */
static void
print_help(void)
{
	print_usage();
	print_version();
	printf(help_str, DAEMON_NAME);
}

int
main(int argc, char *argv[])
{
	int opt;
	int option_index;

	struct rpmemd_options opts;
	rpmemd_options_default(&opts);

	while ((opt = getopt_long(argc, argv, optstr,
			long_options, &option_index)) != -1) {
		switch (opt) {
		case 'V':
			print_version();
			return 0;
		case 'h':
			print_help();
			return 0;
		case 'f':
			opts.foreground = true;
			break;
		case 'o':
			opts.log_file = optarg;
			break;
		case 'l':
			rpmemd_log_level = rpmemd_log_level_from_str(optarg);
			if (rpmemd_log_level == MAX_RPD_LOG) {
				fprintf(stderr, "invalid log level "
					"specified -- '%s'\n", optarg);
				return 1;
			}
			break;
		case 'v':
			if (rpmemd_log_level < MAX_RPD_LOG - 1)
				rpmemd_log_level++;
			break;
		case OPT_SYSLOG:
			opts.use_syslog = true;
			break;
		default:
			print_usage();
			return -1;
		}
	}

	if (opts.foreground)
		rpmemd_log_init(DAEMON_NAME, NULL, 0);
	else
		rpmemd_log_init(DAEMON_NAME, opts.log_file, opts.use_syslog);

	RPMEMD_LOG(INFO, "%s version %s\n", DAEMON_NAME, SRCVERSION);
	if (!opts.foreground) {
		if (daemon(0, 0) < 0) {
			RPMEMD_FATAL("!daemon");
		}
	}

	while (1) {
		/* XXX - placeholder */
	}

	rpmemd_log_close();

	return 0;
}
