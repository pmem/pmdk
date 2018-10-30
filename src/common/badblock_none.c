/*
 * Copyright 2018, Intel Corporation
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
 * badblock_none.c - fake bad block API
 */

#include <errno.h>

#include "out.h"
#include "os_badblock.h"

/*
 * os_badblocks_check_file -- check if the file contains bad blocks
 *
 * Return value:
 * -1 : an error
 *  0 : no bad blocks
 *  1 : bad blocks detected
 */
int
os_badblocks_check_file(const char *file)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_count -- returns number of bad blocks in the file
 *                       or -1 in case of an error
 */
long
os_badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_get -- returns list of bad blocks in the file
 */
int
os_badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_clear -- clears the given bad blocks in a file
 *                       (regular file or dax device)
 */
int
os_badblocks_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}

/*
 * os_badblocks_clear_all -- clears all bad blocks in a file
 *                           (regular file or dax device)
 */
int
os_badblocks_clear_all(const char *file)
{
	LOG(3, "file %s", file);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}
