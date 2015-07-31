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
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY LOG OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * parser.c -- parser of set files
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <sys/queue.h>

#include "util.h"
#include "out.h"
#include "parser.h"

enum parser_codes {
	PARSER_CONTINUE = 0,
	PARSER_PMEMPOOLSET,
	PARSER_REPLICA,
	PARSER_SIZE_PATH_EXPECTED,
	PARSER_WRONG_SIZE,
	PARSER_WRONG_PATH,
	PARSER_SET_NO_PARTS,
	PARSER_REP_NO_PARTS,
	PARSER_SIZE_MISMATCH,
	PARSER_FORMAT_OK,
	PARSER_MAX_CODE
};

static const char *parser_errstr[PARSER_MAX_CODE] = {
	"", /* parsing */
	"the first line must be exactly 'PMEMPOOLSET'",
	"exactly 'REPLICA' expected",
	"size and path expected",
	"incorrect format of size",
	"incorrect path (must be an absolute path)",
	"no pool set parts",
	"no replica parts",
	"sizes of pool set and replica mismatch",
	"" /* format correct */
};

/*
 * parser_get_next_token -- (internal) extract token from string
 */
static char *
parser_get_next_token(char **line)
{
	while (*line && isblank(**line))
		(*line)++;

	return strsep(line, " \t");
}

/*
 * parser_read_line -- (internal) read line and validate size and path
 *                      from a pool set file
 */
static enum parser_codes
parser_read_line(char *line, size_t *size, char **path)
{
	char *size_str, *path_str, *endptr;

	size_str = parser_get_next_token(&line);
	path_str = parser_get_next_token(&line);

	if (!size_str || !path_str)
		return PARSER_SIZE_PATH_EXPECTED;

	LOG(10, "size '%s' path '%s'", size_str, path_str);

	/*
	 * A format of the size is checked in detail. As regards the path,
	 * it is checked only if the read path is an absolute path.
	 * The rest should be checked during creating/opening the file.
	 */

	/* check if the read path is an absolute path */
	if (path_str[0] != '/')
		return PARSER_WRONG_PATH; /* must be an absolute path */

	*path = Strdup(path_str);

	/* check format of size */
	*size = strtoull(size_str, &endptr, 10);
	if (endptr && endptr[0] != '\0') {
		if (endptr[1] != '\0' || strchr("KMGTkmgt", endptr[0]) == NULL)
			return PARSER_WRONG_SIZE; /* wrong format of size */
		/* multiply size by a unit */
		switch (endptr[0]) {
			case 'k':
			case 'K':
				*size *= 1ULL << 10; /* 1 KB */
				break;
			case 'm':
			case 'M':
				*size *= 1ULL << 20; /* 1 MB */
				break;
			case 'g':
			case 'G':
				*size *= 1ULL << 30; /* 1 GB */
				break;
			case 't':
			case 'T':
				*size *= 1ULL << 40; /* 1 TB */
				break;
		}
	}

	return PARSER_CONTINUE;
}

/*
 * parser_add_part -- (internal) add a new part to the list
 */
static void
parser_add_part(struct headparts *headp, int id, size_t size, char *path)
{
	struct part *p = Malloc(sizeof (*p));
	if (p == NULL) {
		FATAL("!Malloc");
	}
	p->id = id;
	p->size = size;
	p->path = path;
	TAILQ_INSERT_TAIL(headp, p, next);
}

/*
 * parser_add_replica -- (internal) add a new replica to the list
 */
static struct replica *
parser_add_replica(struct headreps *headr, int id)
{
	struct replica *r = Malloc(sizeof (*r));
	if (r == NULL) {
		FATAL("!Malloc");
	}
	r->rep_id = id;
	r->nparts = 0;
	r->rep_size = 0;
	TAILQ_INIT(&r->rep_parts);
	TAILQ_INSERT_TAIL(headr, r, next);
	return TAILQ_LAST(headr, headreps);
}

/*
 * parser_init_poolset -- init a pool set structure
 */
void
parser_init_poolset(struct poolset *ps)
{
	TAILQ_INIT(&ps->set_parts);
	TAILQ_INIT(&ps->reps);
	ps->nparts = 0;
	ps->set_size = 0;
	ps->nreps = 0;
}

/*
 * parser_free_poolset -- free a pool set structure
 */
void
parser_free_poolset(struct poolset *ps)
{
	struct part *p;
	struct replica *r;

	/* free pool set parts */
	while (!TAILQ_EMPTY(&ps->set_parts)) {
		p = TAILQ_FIRST(&ps->set_parts);
		TAILQ_REMOVE(&ps->set_parts, p, next);
		Free(p->path);
		Free(p);
	}

	/* free all replicas */
	while (!TAILQ_EMPTY(&ps->reps)) {
		r = TAILQ_FIRST(&ps->reps);
		/* free parts of the current replica */
		while (!TAILQ_EMPTY(&r->rep_parts)) {
			p = TAILQ_FIRST(&r->rep_parts);
			TAILQ_REMOVE(&r->rep_parts, p, next);
			Free(p->path);
			Free(p);
		}
		TAILQ_REMOVE(&ps->reps, r, next);
		Free(r);
	}
}

/*
 * parser_parse_set_file -- parse a pool set file
 */
int
parser_parse_set_file(const char *path, int fd, struct poolset *ps)
{
	LOG(4, "parsing file %s", path);

#define	REPLICA_HDR_SIG "REPLICA"
#define	REPLICA_HDR_SIG_LEN 7
#define	PARSER_MAX_LINE 4160 /* 4096 for path + 64 for size and whitespaces */

	enum parser_states_enum {
		STATE_BEGIN,
		STATE_PMEMPOOLSET,
		STATE_REPLICA
	} parser_state;

	enum parser_codes result;
	char line[PARSER_MAX_LINE];
	char *s, *ppath, *cp;
	struct replica *rep = NULL;
	size_t psize;
	FILE *fs;

	/* associate a stream with the file descriptor */
	if ((fs = fdopen(fd, "r")) == NULL) {
		ERR("!fdopen %s", path);
		return -1;
	}

	int nlines = 0;
	int nparts = 0;
	size_t set_size = 0;
	size_t rep_size = 0;
	parser_state = STATE_BEGIN;
	result = PARSER_CONTINUE;
	while (result == PARSER_CONTINUE) {
		/* read next line */
		s = fgets(line, PARSER_MAX_LINE, fs);
		nlines++;

		if (s) {
			/* skip comments and blank lines */
			if (*s == '#' || *s == '\n')
				continue;

			/* chop off newline */
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
		}

		switch (parser_state) {
		case STATE_BEGIN:
			/* compare also if the last character is '\0' */
			if (s && strncmp(line, POOLSET_HDR_SIG,
						POOLSET_HDR_SIG_LEN + 1) == 0) {
				/* 'PMEMPOOLSET' signature detected */
				LOG(10, "PMEMPOOLSET");
				nparts = 0;
				parser_state = STATE_PMEMPOOLSET;
				result = PARSER_CONTINUE;
			} else {
				result = PARSER_PMEMPOOLSET;
			}
			break;

		case STATE_PMEMPOOLSET:
			if (!s) {
				if (nparts >= 1) {
					result = PARSER_FORMAT_OK;
				} else {
					result = PARSER_SET_NO_PARTS;
				}
			} else if (strncmp(line, REPLICA_HDR_SIG,
						REPLICA_HDR_SIG_LEN) == 0) {
				if (line[REPLICA_HDR_SIG_LEN] != '\0') {
					/* something more than 'REPLICA' */
					result = PARSER_REPLICA;
				} else if (nparts >= 1) {
					/* 'REPLICA' signature detected */
					LOG(10, "REPLICA");

					/* add the first replica to the list */
					rep = parser_add_replica(
							&ps->reps, ps->nreps++);
					nparts = 0;

					parser_state = STATE_REPLICA;
					result = PARSER_CONTINUE;
				} else {
					result = PARSER_SET_NO_PARTS;
				}
			} else {
				/* read size and path */
				result = parser_read_line(line, &psize,
								&ppath);
				if (result == PARSER_CONTINUE) {
					/* add a new pool's part to the list */
					parser_add_part(&ps->set_parts,
							nparts, psize, ppath);
					nparts++;
					set_size += psize;

					/* save pool's part info */
					ps->nparts = nparts;
					ps->set_size = set_size;
				}
			}
			break;

		case STATE_REPLICA:
			if (!s) {
				if (nparts >= 1) {
					if (rep_size == set_size) {
						result = PARSER_FORMAT_OK;
					} else {
						result = PARSER_SIZE_MISMATCH;
					}
				} else {
					result = PARSER_REP_NO_PARTS;
				}
			} else if (strncmp(line, REPLICA_HDR_SIG,
						REPLICA_HDR_SIG_LEN) == 0) {
				if (line[REPLICA_HDR_SIG_LEN] != '\0') {
					/* something more than 'REPLICA' */
					result = PARSER_REPLICA;
				} else if (nparts >= 1) {
					/* 'REPLICA' signature detected */
					LOG(10, "REPLICA");

					if (rep_size != set_size) {
						result = PARSER_SIZE_MISMATCH;
						break;
					}

					/* add next replica to the list */
					rep = parser_add_replica(
							&ps->reps, ps->nreps++);
					nparts = 0;
					rep_size = 0;

					result = PARSER_CONTINUE;
				} else {
					result = PARSER_REP_NO_PARTS;
				}
			} else {
				/* read size and path */
				result = parser_read_line(line, &psize,
								&ppath);
				if (result == PARSER_CONTINUE) {
					/* add new replica's part to the list */
					parser_add_part(&rep->rep_parts,
							nparts, psize, ppath);
					nparts++;
					rep_size += psize;

					/* save replica's part info */
					rep->nparts = nparts;
					rep->rep_size = rep_size;
				}
			}
			break;
		}

	}

	fclose(fs);

	if (result == PARSER_FORMAT_OK) {
		LOG(4, "set file format correct (%s)", path);
		return 0;
	} else {
		ERR("%s [%s:%d]", parser_errstr[result], path, nlines);
		return -1;
	}

#undef	PARSER_MAX_LINE
#undef	REPLICA_HDR_SIG
#undef	REPLICA_HDR_SIG_LEN
}
