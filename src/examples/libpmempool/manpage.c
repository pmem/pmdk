/*
 * Copyright 2016, Intel Corporation
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
 * manpage.c -- simple example for the libpmempool man page
 */

#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <libpmempool.h>

#define PATH "./pmem-fs/myfile"
#define CHECK_FLAGS (PMEMPOOL_CHECK_FORMAT_STR|PMEMPOOL_CHECK_REPAIR|\
		PMEMPOOL_CHECK_VERBOSE)

int
main(int argc, char *argv[])
{
	PMEMpoolcheck *ppc;
	struct pmempool_check_status *status;
	enum pmempool_check_result ret;

	/* arguments for check */
	struct pmempool_check_args args = {
		.path		= PATH,
		.backup_path	= NULL,
		.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		.flags		= CHECK_FLAGS
	};

	/* initialize check context */
	if ((ppc = pmempool_check_init(&args, sizeof(args))) == NULL) {
		perror("pmempool_check_init");
		exit(EXIT_FAILURE);
	}

	/* perform check and repair, answer 'yes' for each question */
	while ((status = pmempool_check(ppc)) != NULL) {
		switch (status->type) {
		case PMEMPOOL_CHECK_MSG_TYPE_ERROR:
			printf("%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_INFO:
			printf("%s\n", status->str.msg);
			break;
		case PMEMPOOL_CHECK_MSG_TYPE_QUESTION:
			printf("%s\n", status->str.msg);
			status->str.answer = "yes";
			break;
		default:
			pmempool_check_end(ppc);
			exit(EXIT_FAILURE);
		}
	}

	/* finalize the check and get the result */
	ret = pmempool_check_end(ppc);
	switch (ret) {
		case PMEMPOOL_CHECK_RESULT_CONSISTENT:
		case PMEMPOOL_CHECK_RESULT_REPAIRED:
			return 0;
		default:
			return 1;
	}
}
