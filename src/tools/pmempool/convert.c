// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2023, Intel Corporation */

/*
 * convert.c -- pmempool convert command source file
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "convert.h"
#include "os.h"

static const char *delimiter = ":";
static const char *convert_bin = "/pmdk-convert";

static int
pmempool_convert_get_path(char *p, size_t max_len)
{
	char *path = strdup(os_getenv("PATH"));
	if (!path) {
		perror("strdup");
		return -1;
	}

	char *dir = strtok(path, delimiter);

	while (dir) {
		size_t length = strlen(dir) + strlen(convert_bin) + 1;
		if (length > max_len) {
			fprintf(stderr, "very long dir in PATH, ignoring\n");
			continue;
		}

		strcpy(p, dir);
		strcat(p, convert_bin);

		if (os_access(p, F_OK) == 0) {
			free(path);
			return 0;
		}

		dir = strtok(NULL, delimiter);
	}

	free(path);
	return -1;
}

/*
 * pmempool_convert_help -- print help message for convert command. This is
 * help message from pmdk-convert tool.
 */
void
pmempool_convert_help(const char *appname)
{
	char path[4096];
	if (pmempool_convert_get_path(path, sizeof(path))) {
		fprintf(stderr,
			"pmdk-convert is not installed. Please install it.\n");
		exit(1);
	}

	char *args[] = { path, "-h", NULL };

	os_execv(path, args);

	perror("execv");
	exit(1);
}

/*
 * pmempool_convert_func -- main function for convert command.
 * It invokes pmdk-convert tool.
 */
int
pmempool_convert_func(const char *appname, int argc, char *argv[])
{
	char path[4096];
	if (pmempool_convert_get_path(path, sizeof(path))) {
		fprintf(stderr,
			"pmdk-convert is not installed. Please install it.\n");
		exit(1);
	}

	char **args = malloc(((size_t)argc + 1) * sizeof(*args));
	if (!args) {
		perror("malloc");
		exit(1);
	}

	args[0] = path;
	for (int i = 1; i < argc; ++i)
		args[i] = argv[i];
	args[argc] = NULL;

	os_execv(args[0], args);

	perror("execv");
	free(args);
	exit(1);
}
