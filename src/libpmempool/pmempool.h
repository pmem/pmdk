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
 * pmempool.h -- internal definitions for libpmempool
 */

#define PMEMPOOL_LOG_PREFIX "libpmempool"
#define PMEMPOOL_LOG_LEVEL_VAR "PMEMPOOL_LOG_LEVEL"
#define PMEMPOOL_LOG_FILE_VAR "PMEMPOOL_LOG_FILE"

extern unsigned long long Pagesize;

enum check_result {
	CHECK_RESULT_CONSISTENT,
	CHECK_RESULT_NOT_CONSISTENT,
	CHECK_RESULT_ASK_QUESTIONS,
	CHECK_RESULT_PROCESS_ANSWERS,
	CHECK_RESULT_REPAIRED,
	CHECK_RESULT_CANNOT_REPAIR,
	CHECK_RESULT_ERROR,
	CHECK_RESULT_INTERNAL_ERROR
};

/*
 * pmempool_check -- context and arguments for check command
 */
struct pmempool_check {
	struct pmempool_check_args args;
	char *path;
	char *backup_path;

	struct check_data *data;
	struct pool_data *pool;
	enum check_result result;
};
