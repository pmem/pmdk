/*
 * Copyright 2014-2018, Intel Corporation
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "convert.h"
#include <stdint.h>

#ifdef _WIN32
static const char *delimeter = ";";
static const char *convert_bin = "\\pmdk-convert.exe";
#else
static const char *delimeter = ":";
static const char *convert_bin = "/pmdk-convert";
#endif // _WIN32

char
*pmempool_convert_get_path()
{
	char *paths = strtok(getenv("PATH"), delimeter);
	char *temp;
	size_t length;

	while (paths) {
		length = strlen(paths) + strlen(convert_bin) + 1;
		temp = malloc(length);

		strcpy(temp, paths);
		strcat(temp, convert_bin);

		if (access(temp, F_OK) == 0) {
			return temp;
		}

		paths = strtok(NULL, delimeter);
		free(temp);
	}

	return NULL;
}

/*
 * pmempool_convert_help -- print help message for convert command. This is
 * help message from pmdk-convert tool.
 */
void
pmempool_convert_help(char *appname)
{
	char *path = pmempool_convert_get_path();
	if (!path) {
		printf(
			"Pmdk-convert is not installed. Please install it.\n");
		exit(1);
	}

	char *args[] = { path, "-h", NULL };
	char *envp[] = { NULL };

	int ret = (int)execve(path, args, envp);
	if (ret) {
		printf("Pmempool convert failed: %s\n", strerror(errno));
		exit(1);
	}

	free(path);
}

/*
 * pmempool_convert_func -- main function for convert command.
 * It invokes pmdk-convert tool.
 */
int
pmempool_convert_func(char *appname, int argc, char *argv[])
{
	char *path = pmempool_convert_get_path();
	if (!path) {
		printf(
			"Pmdk-convert is not installed. Please install it.\n");
		exit(1);
	}
	char *envp[] = { NULL };

	char **args = calloc((uint32_t)(argc) + 1, sizeof(*args));
	args[0] = strdup(path);
	if (args[0] == NULL) {
		printf("Error: %s\n", strerror(errno));
		exit(1);
	}

	for (int i = 1; i < argc; ++i) {
		args[i] = strdup(argv[i]);
		if (args[i] == NULL) {
			printf("Error: %s\n", strerror(errno));
			exit(2);
		}
	}

	args[argc] = NULL;
	int ret = (int)execve(args[0], args, envp);
	if (ret == -1) {
		printf("Pmempool convert failed: %s\n", strerror(errno));
		exit(1);
	}

	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
	free(args);
	free(path);

	return 0;
}
