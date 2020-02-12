// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * getopt.c -- test for windows getopt() implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "unittest.h"

/*
 * long_options -- command line arguments
 */
static const struct option long_options[] = {
	{ "arg_a",	no_argument,		NULL,	'a' },
	{ "arg_b",	no_argument,		NULL,	'b' },
	{ "arg_c",	no_argument,		NULL,	'c' },
	{ "arg_d",	no_argument,		NULL,	'd' },
	{ "arg_e",	no_argument,		NULL,	'e' },
	{ "arg_f",	no_argument,		NULL,	'f' },
	{ "arg_g",	no_argument,		NULL,	'g' },
	{ "arg_h",	no_argument,		NULL,	'h' },
	{ "arg_A",	required_argument,	NULL,	'A' },
	{ "arg_B",	required_argument,	NULL,	'B' },
	{ "arg_C",	required_argument,	NULL,	'C' },
	{ "arg_D",	required_argument,	NULL,	'D' },
	{ "arg_E",	required_argument,	NULL,	'E' },
	{ "arg_F",	required_argument,	NULL,	'F' },
	{ "arg_G",	required_argument,	NULL,	'G' },
	{ "arg_H",	required_argument,	NULL,	'H' },
	{ "arg_1",	optional_argument,	NULL,	'1' },
	{ "arg_2",	optional_argument,	NULL,	'2' },
	{ "arg_3",	optional_argument,	NULL,	'3' },
	{ "arg_4",	optional_argument,	NULL,	'4' },
	{ "arg_5",	optional_argument,	NULL,	'5' },
	{ "arg_6",	optional_argument,	NULL,	'6' },
	{ "arg_7",	optional_argument,	NULL,	'7' },
	{ "arg_8",	optional_argument,	NULL,	'8' },
	{ NULL,		0,			NULL,	 0 },
};

int
main(int argc, char *argv[])
{
	int opt;
	int option_index;

	START(argc, argv, "getopt");

	while ((opt = getopt_long(argc, argv,
			"abcdefghA:B:C:D:E:F:G::H1::2::3::4::5::6::7::8::",
			long_options, &option_index)) != -1) {
		switch (opt) {
		case '?':
			UT_OUT("unknown argument");
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
		case 'h':
			UT_OUT("arg_%c", opt);
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
		case 'G':
		case 'H':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
			UT_OUT("arg_%c=%s", opt,
				optarg == NULL ? "null": optarg);
			break;
		}
	}
	while (optind < argc) {
		UT_OUT("%s", argv[optind++]);
	}
	DONE(NULL);
}
