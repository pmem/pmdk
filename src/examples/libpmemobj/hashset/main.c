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

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <libpmemobj.h>

#include "hashset.h"

#define	PM_HASHSET_POOL_SIZE	(8 * 1024 * 1024)

static PMEMobjpool *pop;

/*
 * str_insert -- hs_insert wrapper which works on strings
 */
static void
str_insert(const char *str)
{
	uint64_t val;
	if (sscanf(str, "%lu", &val) > 0)
		hs_insert(pop, val);
	else
		fprintf(stderr, "insert: invalid syntax\n");
}

/*
 * str_remove -- hs_remove wrapper which works on strings
 */
static void
str_remove(const char *str)
{
	uint64_t val;
	if (sscanf(str, "%lu", &val) > 0) {
		int r = hs_remove(pop, val);
		if (r == 0)
			fprintf(stderr, "no such value\n");
	} else
		fprintf(stderr,	"remove: invalid syntax\n");
}

/*
 * str_check -- hs_check wrapper which works on strings
 */
static void
str_check(const char *str)
{
	uint64_t val;
	if (sscanf(str, "%lu", &val) > 0)
		printf("%d\n", hs_check(pop, val));
	else
		fprintf(stderr, "check: invalid syntax\n");
}

/*
 * str_insert_random -- inserts specified (as string) number of random numbers
 */
static void
str_insert_random(const char *str)
{
	uint64_t val;
	if (sscanf(str, "%lu", &val) > 0)
		for (uint64_t i = 0; i < val; ) {
			uint64_t r = ((uint64_t)rand()) << 32 | rand();
			int ret = hs_insert(pop, r);
			if (ret < 0)
				break;
			i += ret;
		}
	else
		fprintf(stderr, "random insert: invalid syntax\n");
}

#ifdef DEBUG
/*
 * str_rebuild -- hs_rebuild wrapper which executes specified number of times
 */
static void
str_rebuild(const char *str)
{
	uint64_t val;

	if (sscanf(str, "%lu", &val) > 0) {
		for (uint64_t i = 0; i < val; ++i) {
			printf("%2lu ", i);
			hs_rebuild(pop, 0);
		}
	}
	else
		hs_rebuild(pop, 0);
}
#endif

static void
help(void)
{
	printf("h - help\n");
	printf("i $value - insert $value\n");
	printf("r $value - remove $value\n");
	printf("c $value - check $value, returns 0/1\n");
	printf("n $value - insert $value random values\n");
	printf("p - print all values\n");
	printf("d - print debug info\n");
#ifdef DEBUG
	printf("b [$value] - rebuild $value (default: 1) times\n");
#endif
	printf("q - quit\n");
}

static void
unknown_command(const char *str)
{
	fprintf(stderr, "unknown command '%c', use 'h' for help\n", str[0]);
}

#define	INPUT_BUF_LEN 1000
int
main(int argc, char *argv[])
{
	if (argc < 2) {
		printf("usage: %s file-name\n", argv[0]);
		return 1;
	}

	const char *path = argv[1];

	if (access(path, F_OK) != 0) {
		pop = pmemobj_create(path, hs_layout_name(),
				PM_HASHSET_POOL_SIZE, S_IRUSR | S_IWUSR);
		if (pop == NULL) {
			fprintf(stderr, "failed to create pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}

		uint32_t seed;

		if (argc > 2)
			seed = atoi(argv[2]);
		else
			seed = time(NULL);
		srand(seed);

		printf("seed: %u\n", seed);
		hs_create(pop, seed);
	} else {
		pop = pmemobj_open(path, hs_layout_name());
		if (pop == NULL) {
			fprintf(stderr, "failed to open pool: %s\n",
					pmemobj_errormsg());
			return 1;
		}

		hs_init(pop);
	}

	char buf[INPUT_BUF_LEN];
	printf("Type 'h' for help\n$ ");
	while (fgets(buf, sizeof (buf), stdin)) {
		if (buf[0] == 0 || buf[0] == '\n')
			continue;

		switch (buf[0]) {
			case 'i':
				str_insert(buf + 1);
				break;
			case 'r':
				str_remove(buf + 1);
				break;
			case 'c':
				str_check(buf + 1);
				break;
			case 'n':
				str_insert_random(buf + 1);
				break;
			case 'p':
				hs_print(pop);
				break;
			case 'd':
				hs_debug(pop);
				break;
#ifdef DEBUG
			case 'b':
				str_rebuild(buf + 1);
				break;
#endif
			case 'q':
				fclose(stdin);
				break;
			case 'h':
				help();
				break;
			default:
				unknown_command(buf);
				break;
		}

		printf("$ ");
	}

	pmemobj_close(pop);

	return 0;
}
