/*
 * Copyright 2016-2019, Intel Corporation
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * check-license.c -- check the license in the file
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#define LICENSE_MAX_LEN		2048
#define COPYRIGHT		"Copyright "
#define COPYRIGHT_LEN		10
#define COPYRIGHT_SYMBOL	"(c) "
#define COPYRIGHT_SYMBOL_LEN	4
#define YEAR_MIN		1900
#define YEAR_MAX		9999
#define YEAR_INIT_MIN		9999
#define YEAR_INIT_MAX		0
#define YEAR_LEN		4
#define LICENSE_BEG		"Redistribution and use"
#define LICENSE_END		"THE POSSIBILITY OF SUCH DAMAGE."
#define DIFF_LEN		50
#define COMMENT_STR_LEN		5

#define STR_MODE_CREATE		"create"
#define STR_MODE_PATTERN	"check-pattern"
#define STR_MODE_LICENSE	"check-license"

#define ERROR(fmt, ...)	fprintf(stderr, "error: " fmt "\n", __VA_ARGS__)
#define ERROR2(fmt, ...)	fprintf(stderr, fmt "\n", __VA_ARGS__)

/*
 * help_str -- string for the help message
 */
static const char * const help_str =
"Usage: %s <mode> <file_1> <file_2> [filename]\n"
"\n"
"Modes:\n"
"   create <file_license> <file_pattern>\n"
"     - create a license pattern file <file_pattern>\n"
"       from the license text file <file_license>\n"
"\n"
"   check-pattern <file_pattern> <file_to_check>\n"
"     - check if a license in <file_to_check>\n"
"       matches the license pattern in <file_pattern>,\n"
"       if it does, copyright dates are printed out (see below)\n"
"\n"
"   check-license <file_license> <file_to_check>\n"
"     - check if a license in <file_to_check>\n"
"       matches the license text in <file_license>,\n"
"       if it does, copyright dates are printed out (see below)\n"
"\n"
"In case of 'check_pattern' and 'check_license' modes,\n"
"if the license is correct, it prints out copyright dates\n"
"in the following format: OLDEST_YEAR-NEWEST_YEAR\n"
"\n"
"Return value: returns 0 on success and -1 on error.\n"
"\n";

/*
 * read_pattern -- read the pattern from the 'path_pattern' file to 'pattern'
 */
static int
read_pattern(const char *path_pattern, char *pattern)
{
	int file_pattern;
	ssize_t ret;

	if ((file_pattern = open(path_pattern, O_RDONLY)) == -1) {
		ERROR("open(): %s: %s", strerror(errno), path_pattern);
		return -1;
	}

	ret = read(file_pattern, pattern, LICENSE_MAX_LEN);
	close(file_pattern);

	if (ret == -1) {
		ERROR("read(): %s: %s", strerror(errno), path_pattern);
		return -1;
	} else if (ret != LICENSE_MAX_LEN) {
		ERROR("read(): incorrect format of the license pattern"
			" file (%s)", path_pattern);
		return -1;
	}
	return 0;
}

/*
 * write_pattern -- write 'pattern' to the 'path_pattern' file
 */
static int
write_pattern(const char *path_pattern, char *pattern)
{
	int file_pattern;
	ssize_t ret;

	if ((file_pattern = open(path_pattern, O_WRONLY | O_CREAT | O_EXCL,
					S_IRUSR | S_IRGRP | S_IROTH)) == -1) {
		ERROR("open(): %s: %s", strerror(errno), path_pattern);
		return -1;
	}

	ret = write(file_pattern, pattern, LICENSE_MAX_LEN);
	close(file_pattern);

	if (ret < LICENSE_MAX_LEN) {
		ERROR("write(): %s: %s", strerror(errno), path_pattern);
		return -1;
	}

	return 0;
}

/*
 * strstr2 -- locate two substrings in the string
 */
static int
strstr2(const char *str, const char *sub1, const char *sub2,
		char **pos1, char **pos2)
{
	*pos1 = strstr(str, sub1);
	*pos2 = strstr(str, sub2);
	if (*pos1 == NULL || *pos2 == NULL)
		return -1;
	return 0;
}

/*
 * format_license -- remove comments and redundant whitespaces from the license
 */
static void
format_license(char *license, size_t length)
{
	char comment_str[COMMENT_STR_LEN];
	char *comment = license;
	size_t comment_len;
	int was_space;
	size_t w, r;

	/* detect a comment string */
	while (*comment != '\n')
		comment--;
	/* is there any comment? */
	if (comment + 1 != license) {
		/* separate out a comment */
		strncpy(comment_str, comment, COMMENT_STR_LEN - 1);
		comment_str[COMMENT_STR_LEN - 1] = 0;
		comment = comment_str + 1;
		while (isspace(*comment))
			comment++;
		while (!isspace(*comment))
			comment++;
		*comment = '\0';
		comment_len = strlen(comment_str);

		/* replace comments with spaces */
		if (comment_len > 2) {
			while ((comment = strstr(license, comment_str)) != NULL)
				for (w = 1; w < comment_len; w++)
					comment[w] = ' ';
		} else {
			while ((comment = strstr(license, comment_str)) != NULL)
				comment[1] = ' ';
		}
	}

	/* replace multiple spaces with one space */
	was_space = 0;
	for (r = w = 0; r < length; r++) {
		if (!isspace(license[r])) {
			if (was_space) {
				license[w++] = ' ';
				was_space = 0;
			}
			if (w < r)
				license[w] = license[r];
			w++;
		} else {
			if (!was_space)
				was_space = 1;
		}
	}
	license[w] = '\0';
}

/*
 * analyze_license -- check correctness of the license
 */
static int
analyze_license(const char *name_to_print,
		char *buffer,
		char **license)
{
	char *_license;
	size_t _length;
	char *beg_str, *end_str;

	if (strstr2(buffer, LICENSE_BEG, LICENSE_END,
				&beg_str, &end_str)) {
		if (!beg_str)
			ERROR2("%s:1: error: incorrect license"
				" (license should start with the string '%s')",
				name_to_print, LICENSE_BEG);
		else
			ERROR2("%s:1: error: incorrect license"
				" (license should end with the string '%s')",
				name_to_print, LICENSE_END);
		return -1;
	}

	_license = beg_str;
	assert((uintptr_t)end_str > (uintptr_t)beg_str);
	_length = (size_t)(end_str - beg_str) + strlen(LICENSE_END);
	_license[_length] = '\0';

	format_license(_license, _length);

	*license = _license;

	return 0;
}

/*
 * create_pattern -- create 'pattern' from the 'path_license' file
 */
static int
create_pattern(const char *path_license, char *pattern)
{
	char buffer[LICENSE_MAX_LEN];
	char *license;
	ssize_t ret;
	int file_license;

	if ((file_license = open(path_license, O_RDONLY)) == -1) {
		ERROR("open(): %s: %s", strerror(errno), path_license);
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));
	ret = read(file_license, buffer, LICENSE_MAX_LEN);
	close(file_license);

	if (ret == -1) {
		ERROR("read(): %s: %s", strerror(errno), path_license);
		return -1;
	}

	if (analyze_license(path_license, buffer, &license) == -1)
		return -1;

	strncpy(pattern, license, LICENSE_MAX_LEN);

	return 0;
}

/*
 * print_diff -- print the first difference between 'license' and 'pattern'
 */
static void
print_diff(char *license, char *pattern, size_t len)
{
	size_t i = 0;

	while (i < len && license[i] == pattern[i])
		i++;
	license[i + 1] = '\0';
	pattern[i + 1] = '\0';

	i = (i - DIFF_LEN > 0) ? (i - DIFF_LEN) : 0;
	while (i > 0 && license[i] != ' ')
		i--;

	fprintf(stderr, "   The first difference is at the end of the line:\n");
	fprintf(stderr, "   * License: %s\n", license + i);
	fprintf(stderr, "   * Pattern: %s\n", pattern + i);
}

/*
 * verify_license -- compare 'license' with 'pattern' and check correctness
 *                  of the copyright line
 */
static int
verify_license(const char *path_to_check, char *pattern, const char *filename)
{
	char buffer[LICENSE_MAX_LEN];
	char *license, *copyright;
	int file_to_check;
	ssize_t ret;
	int year_first, year_last;
	int min_year_first = YEAR_INIT_MIN;
	int max_year_last = YEAR_INIT_MAX;
	char *err_str = NULL;
	const char *name_to_print = filename ? filename : path_to_check;

	if ((file_to_check = open(path_to_check, O_RDONLY)) == -1) {
		ERROR("open(): %s: %s", strerror(errno), path_to_check);
		return -1;
	}

	memset(buffer, 0, sizeof(buffer));
	ret = read(file_to_check, buffer, LICENSE_MAX_LEN);
	close(file_to_check);

	if (ret == -1) {
		ERROR("read(): %s: %s", strerror(errno), name_to_print);
		return -1;
	}

	if (analyze_license(name_to_print, buffer, &license) == -1)
		return -1;

	/* check the copyright notice */
	copyright = buffer;
	while ((copyright = strstr(copyright, COPYRIGHT)) != NULL) {
		copyright += COPYRIGHT_LEN;

		/* skip the copyright symbol '(c)' if any */
		if (strncmp(copyright, COPYRIGHT_SYMBOL,
			COPYRIGHT_SYMBOL_LEN) == 0)
			copyright += COPYRIGHT_SYMBOL_LEN;

		/* look for the first year */
		if (!isdigit(*copyright)) {
			err_str = "no digit just after the 'Copyright ' string";
			break;
		}

		year_first = atoi(copyright);
		if (year_first < YEAR_MIN || year_first > YEAR_MAX) {
			err_str = "the first year is wrong";
			break;
		}
		copyright += YEAR_LEN;

		if (year_first < min_year_first)
			min_year_first = year_first;
		if (year_first > max_year_last)
			max_year_last = year_first;

		/* check if there is the second year */
		if (*copyright == ',')
			continue;
		else if (*copyright != '-') {
			err_str = "'-' or ',' expected after the first year";
			break;
		}
		copyright++;

		/* look for the second year */
		if (!isdigit(*copyright)) {
			err_str = "no digit after '-'";
			break;
		}

		year_last = atoi(copyright);
		if (year_last < YEAR_MIN || year_last > YEAR_MAX) {
			err_str = "the second year is wrong";
			break;
		}
		copyright += YEAR_LEN;

		if (year_last > max_year_last)
			max_year_last = year_last;

		if (*copyright != ',') {
			err_str = "',' expected after the second year";
			break;
		}
	}

	if (!err_str && min_year_first == YEAR_INIT_MIN)
		err_str = "no 'Copyright ' string found";

	if (err_str)
		/* found an error in the copyright notice */
		ERROR2("%s:1: error: incorrect copyright notice: %s",
			name_to_print, err_str);

	/* now check the license */
	if (memcmp(license, pattern, strlen(pattern)) != 0) {
		ERROR2("%s:1: error: incorrect license", name_to_print);
		print_diff(license, pattern, strlen(pattern));
		return -1;
	}

	if (err_str)
		return -1;

	/* all checks passed */
	if (min_year_first != max_year_last && max_year_last != YEAR_INIT_MAX) {
		printf("%i-%i\n", min_year_first, max_year_last);
	} else {
		printf("%i\n", min_year_first);
	}

	return 0;
}

/*
 * mode_create_pattern_file -- 'create' mode function
 */
static int
mode_create_pattern_file(const char *path_license, const char *path_pattern)
{
	char pattern[LICENSE_MAX_LEN];

	if (create_pattern(path_license, pattern) == -1)
		return -1;

	return write_pattern(path_pattern, pattern);
}

/*
 * mode_check_pattern -- 'check_pattern' mode function
 */
static int
mode_check_pattern(const char *path_license, const char *path_to_check)
{
	char pattern[LICENSE_MAX_LEN];

	if (create_pattern(path_license, pattern) == -1)
		return -1;

	return verify_license(path_to_check, pattern, NULL);
}

/*
 * mode_check_license -- 'check_license' mode function
 */
static int
mode_check_license(const char *path_pattern, const char *path_to_check,
		const char *filename)
{
	char pattern[LICENSE_MAX_LEN];

	if (read_pattern(path_pattern, pattern) == -1)
		return -1;

	return verify_license(path_to_check, pattern, filename);
}

int
main(int argc, char *argv[])
{
	if (strcmp(argv[1], STR_MODE_CREATE) == 0) {
		if (argc != 4)
			goto invalid_args;

		return mode_create_pattern_file(argv[2], argv[3]);

	} else if (strcmp(argv[1], STR_MODE_PATTERN) == 0) {
		if (argc != 5)
			goto invalid_args;

		return mode_check_license(argv[2], argv[3], argv[4]);

	} else if (strcmp(argv[1], STR_MODE_LICENSE) == 0) {
		if (argc != 4)
			goto invalid_args;

		return mode_check_pattern(argv[2], argv[3]);

	} else {
		ERROR("wrong mode: %s\n", argv[1]);
	}

invalid_args:
	printf(help_str, argv[0]);
	return -1;
}
