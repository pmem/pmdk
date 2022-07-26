/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

/*
 * pmreorder.h -- helper functions for the pmreorder tool
 *
 * This library provides support while using the pmmreorder tool.
 *
 * pmreorder performs a persistent consistency check using a store reordering
 * mechanism
 *
 * See pmreorder(1) for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct markers {
	unsigned markers_no;
	char **markers;
};

/*
 * get_markers - return list of the markers passed by pmreorder
 */
static inline struct markers *
get_markers(char *input)
{
	if (!input)
		return NULL;

	struct markers *log = (struct markers *)malloc(sizeof(struct markers));
	char *delim = "|";

	log->markers_no = 1;
	for (char *s = input; *s != '\0'; s++)
		if (strncmp(s, delim, strlen(delim) == 0))
			log->markers_no++;
	log->markers = (char **)malloc(log->markers_no * sizeof(char *));

	char *token = strtok(input, delim);
	int i = 0;

	while (token != NULL) {
		log->markers[i] = (char *)malloc(strlen(token) * sizeof(char));
		strncpy(log->markers[i], token, strlen(token));
		i++;
		printf(" %s\n", token);
		token = strtok(NULL, delim);
	}

	return log;
}

static inline void
delete_markers(struct markers *log)
{
	for (unsigned i = 0; i < log->markers_no; i++)
		free(log->markers[i]);
	free(log->markers);
	free(log);
}
