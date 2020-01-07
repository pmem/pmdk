// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * obj_pmemlog_macros.c -- alternate pmemlog implementation based on pmemobj
 *
 * usage: obj_pmemlog_macros [co] file [cmd[:param]...]
 *
 *   c - create file
 *   o - open file
 *
 * The "cmd" arguments match the pmemlog functions:
 *   a - append
 *   v - appendv
 *   r - rewind
 *   w - walk
 *   n - nbyte
 *   t - tell
 * "a" and "v" require a parameter string(s) separated by a colon
 */

#include <ex_common.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "libpmemobj.h"
#include "libpmem.h"
#include "libpmemlog.h"

#define POOL_SIZE ((size_t)(1024 * 1024 * 100))

POBJ_LAYOUT_BEGIN(obj_pmemlog_minimal);
POBJ_LAYOUT_TOID(obj_pmemlog_minimal, struct log);
POBJ_LAYOUT_END(obj_pmemlog_minimal);

/* struct log stores the entire log entry */
struct log {
	size_t size;
	char data[];
};

/* structure containing arguments for the alloc constructor */
struct create_args {
	size_t size;
	const void *src;
};

/*
 * create_log_entry -- (internal) constructor for the log entry
 */
static int
create_log_entry(PMEMobjpool *pop, void *ptr, void *arg)
{
	struct log *logptr = ptr;
	struct create_args *carg = arg;
	logptr->size = carg->size;
	pmemobj_persist(pop, &logptr->size, sizeof(logptr->size));
	pmemobj_memcpy_persist(pop, logptr->data, carg->src, carg->size);

	return 0;
}

/*
 * pmemlog_open -- pool open wrapper
 */
PMEMlogpool *
pmemlog_open(const char *path)
{
	return (PMEMlogpool *)pmemobj_open(path,
				POBJ_LAYOUT_NAME(obj_pmemlog_minimal));
}

/*
 * pmemlog_create -- pool create wrapper
 */
PMEMlogpool *
pmemlog_create(const char *path, size_t poolsize, mode_t mode)
{
	return (PMEMlogpool *)pmemobj_create(path,
				POBJ_LAYOUT_NAME(obj_pmemlog_minimal),
				poolsize, mode);
}

/*
 * pool_close -- pool close wrapper
 */
void
pmemlog_close(PMEMlogpool *plp)
{
	pmemobj_close((PMEMobjpool *)plp);
}

/*
 * pmemlog_nbyte -- not available in this implementation
 */
size_t
pmemlog_nbyte(PMEMlogpool *plp)
{
	/* N/A */
	return 0;
}

/*
 * pmemlog_append -- add data to a log memory pool
 */
int
pmemlog_append(PMEMlogpool *plp, const void *buf, size_t count)
{
	PMEMobjpool *pop = (PMEMobjpool *)plp;

	struct create_args args = { count, buf };
	size_t obj_size = sizeof(size_t) + count;
	/* alloc-construct to an internal list */
	PMEMoid obj;
	pmemobj_alloc(pop, &obj, obj_size,
			0, create_log_entry,
			&args);

	return 0;
}

/*
 * pmemlog_appendv -- add gathered data to a log memory pool
 */
int
pmemlog_appendv(PMEMlogpool *plp, const struct iovec *iov, int iovcnt)
{
	PMEMobjpool *pop = (PMEMobjpool *)plp;

	/* append the data */
	for (int i = 0; i < iovcnt; ++i) {

		struct create_args args = { iov[i].iov_len, iov[i].iov_base };
		size_t obj_size = sizeof(size_t) + args.size;
		/* alloc-construct to an internal list */
		pmemobj_alloc(pop, NULL, obj_size,
				0, create_log_entry, &args);
	}

	return 0;
}

/*
 * pmemlog_tell -- not available in this implementation
 */
long long
pmemlog_tell(PMEMlogpool *plp)
{
	/* N/A */
	return 0;
}

/*
 * pmemlog_rewind -- discard all data, resetting a log memory pool to empty
 */
void
pmemlog_rewind(PMEMlogpool *plp)
{
	PMEMobjpool *pop = (PMEMobjpool *)plp;

	PMEMoid iter, next;
	/* go through each list and remove all entries */
	POBJ_FOREACH_SAFE(pop, iter, next) {
		pmemobj_free(&iter);
	}
}

/*
 * pmemlog_walk -- walk through all data in a log memory pool
 *
 * As this implementation holds the size of each entry, the chunksize is ignored
 * and the process_chunk function gets the actual entry length.
 */
void
pmemlog_walk(PMEMlogpool *plp, size_t chunksize,
	int (*process_chunk)(const void *buf, size_t len, void *arg), void *arg)
{
	PMEMobjpool *pop = (PMEMobjpool *)plp;

	PMEMoid iter;
	/* process each allocated object */
	POBJ_FOREACH(pop, iter) {
		struct log *logptr = pmemobj_direct(iter);
		(*process_chunk)(logptr->data, logptr->size, arg);
	}
}

/*
 * process_chunk -- (internal) process function for log_walk
 */
static int
process_chunk(const void *buf, size_t len, void *arg)
{
	char *tmp = malloc(len + 1);
	if (tmp == NULL) {
		fprintf(stderr, "malloc error\n");
		return 0;
	}

	memcpy(tmp, buf, len);
	tmp[len] = '\0';

	printf("log contains:\n");
	printf("%s\n", tmp);
	free(tmp);
	return 1; /* continue */
}

/*
 * count_iovec -- (internal) count the number of iovec items
 */
static int
count_iovec(char *arg)
{
	int count = 1;
	char *pch = strchr(arg, ':');
	while (pch != NULL) {
		++count;
		pch = strchr(++pch, ':');
	}

	return count;
}

/*
 * fill_iovec -- (internal) fill out the iovec
 */
static void
fill_iovec(struct iovec *iov, char *arg)
{
	char *pch = strtok(arg, ":");
	while (pch != NULL) {
		iov->iov_base = pch;
		iov->iov_len = strlen((char *)iov->iov_base);
		++iov;
		pch = strtok(NULL, ":");
	}
}

int
main(int argc, char *argv[])
{

	if (argc < 2) {
		fprintf(stderr, "usage: %s [o,c] file [val...]\n", argv[0]);
		return 1;
	}

	PMEMlogpool *plp;
	if (strncmp(argv[1], "c", 1) == 0) {
		plp = pmemlog_create(argv[2], POOL_SIZE, CREATE_MODE_RW);
	} else if (strncmp(argv[1], "o", 1) == 0) {
		plp = pmemlog_open(argv[2]);
	} else {
		fprintf(stderr, "usage: %s [o,c] file [val...]\n", argv[0]);
		return 1;
	}

	if (plp == NULL) {
		perror("pmemlog_create/pmemlog_open");
		return 1;
	}

	/* process the command line arguments */
	for (int i = 3; i < argc; i++) {
		switch (*argv[i]) {
			case 'a': {
				printf("append: %s\n", argv[i] + 2);
				if (pmemlog_append(plp, argv[i] + 2,
					strlen(argv[i] + 2)))
					fprintf(stderr, "pmemlog_append"
						" error\n");
				break;
			}
			case 'v': {
				printf("appendv: %s\n", argv[i] + 2);
				int count = count_iovec(argv[i] + 2);
				struct iovec *iov = calloc(count,
						sizeof(struct iovec));
				if (iov == NULL) {
					fprintf(stderr, "malloc error\n");
					return 1;
				}
				fill_iovec(iov, argv[i] + 2);
				if (pmemlog_appendv(plp, iov, count))
					fprintf(stderr, "pmemlog_appendv"
						" error\n");
				free(iov);
				break;
			}
			case 'r': {
				printf("rewind\n");
				pmemlog_rewind(plp);
				break;
			}
			case 'w': {
				printf("walk\n");
				pmemlog_walk(plp, 0, process_chunk, NULL);
				break;
			}
			case 'n': {
				printf("nbytes: %zu\n", pmemlog_nbyte(plp));
				break;
			}
			case 't': {
				printf("offset: %lld\n", pmemlog_tell(plp));
				break;
			}
			default: {
				fprintf(stderr, "unrecognized command %s\n",
					argv[i]);
				break;
			}
		};
	}

	/* all done */
	pmemlog_close(plp);

	return 0;
}
