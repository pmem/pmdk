/*
 * Copyright 2017, Intel Corporation
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
 * os_ras_linux.c -- Linux ras abstraction layer
 */
#include <sys/types.h>
#include <libgen.h>
#include <limits.h>
#include <ndctl/libndctl.h>
#include <string.h>
#include "os.h"
#include "os_ras.h"


char **uids;
size_t uids_size;

static size_t uid_it;

uint64_t *uscs;
size_t uscs_size;
static size_t usc_it;

/*
 * os_dimm_uid -- mocked uid length reading function
 */
int
os_dimm_uid_size(const char *path, size_t *len)
{
	if (uid_it < uids_size) {
		*len = strlen(uids[uid_it]) + 1;
	} else {
		return -1;
	}

	return 0;
}

/*
 * os_dimm_uid -- mocked uid reading function
 */
int
os_dimm_uid(const char *path, char *uid)
{
	if (uid_it < uids_size) {
		strcpy(uid, uids[uid_it]);
		uid_it++;
	} else {
		return -1;
	}

	return 0;
}

/*
 * os_dimm_usc -- mocked usc reading function
 */
int
os_dimm_usc(const char *path, uint64_t *usc)
{
	if (usc_it < uscs_size) {
		*usc = uscs[usc_it];
		usc_it++;
	} else {
		return -1;
	}

	return 0;
}
