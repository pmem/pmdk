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
*get_convert_path(char *appname)
{
	char *paths = strtok(getenv("PATH"), delimeter);
	char *temp = NULL;
	size_t length = 0;
	int ret = -1;

	while (paths && ret) {
		ret = 0;
		length = strlen(paths) + strlen(convert_bin) + 1;
		temp = malloc(length);

		strcpy(temp, paths);
		strcat(temp, convert_bin);
		ret = access(temp, F_OK);

		if (!ret)
			return temp;

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
	char *path = get_convert_path(appname);
	if (!path) {
		printf(
			"Pmdk-convert does not installed. Please install this tool.\n");
		exit(1);
	}

	char *args[] = { path, "-h", NULL };
	char *envp[] = { NULL };
	execve(path, args, envp);
}

/*
 * pmempool_convert_func -- main function for convert command.
 * It evokes pmdk-convert tool.
 */
int
pmempool_convert_func(char *appname, int argc, char *argv[])
{
	char *path = get_convert_path(appname);
	if (!path) {
		printf(
			"Pmdk-convert does not installed. Please install this tool.\n");
		exit(1);
	}
	printf("%s", path);
	char *envp[] = { NULL };
	size_t length = 0;

	char **args = calloc((uint32_t)(argc) + 1, sizeof(*args));
	args[0] = malloc(strlen(path) + 1);
	memcpy(args[0], path, strlen(path) + 1);

	for (int i = 1; i < argc; ++i) {
		length = strlen(argv[i]) + 1;
		args[i] = malloc(length);
		memcpy(args[i], argv[i], length);
	}

	args[argc] = NULL;
	execve(args[0], args, envp);

	for (int i = 0; i < argc; ++i) {
		free(args[i]);
	}
	free(args);

	return 0;
}
