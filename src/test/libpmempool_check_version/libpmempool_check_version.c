/*
 * Copyright 2019, Intel Corporation
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
 * libpmempool_check_version -- a unittest for libpmempool_check_version.
 *
 */

#include "unittest.h"
#include "libpmempool.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "libpmempool_check_version");

	UT_ASSERTne(pmempool_check_version(0, 0), NULL);

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION - 1,
		PMEMPOOL_MINOR_VERSION));

	if (PMEMPOOL_MINOR_VERSION > 0) {
		UT_ASSERTeq(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
			PMEMPOOL_MINOR_VERSION - 1));
	}

	UT_ASSERTeq(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
		PMEMPOOL_MINOR_VERSION));

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION + 1,
		PMEMPOOL_MINOR_VERSION));

	UT_ASSERTne(NULL, pmempool_check_version(PMEMPOOL_MAJOR_VERSION,
		PMEMPOOL_MINOR_VERSION + 1));

	DONE(NULL);
}
