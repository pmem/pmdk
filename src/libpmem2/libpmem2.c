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
 * libpmem2.c -- pmem2 entry points for libpmem2
 */

#include <stdio.h>
#include <stdint.h>

#include "libpmem2.h"

#include "pmem2.h"
#include "pmemcommon.h"

/*
 * libpmem2_init -- load-time initialization for libpmem2
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmem2_init(void)
{
	util_init();
	out_init(PMEM2_LOG_PREFIX, PMEM2_LOG_LEVEL_VAR, PMEM2_LOG_FILE_VAR,
			PMEM2_MAJOR_VERSION, PMEM2_MINOR_VERSION);

	/* XXX LOG(3, NULL); placeholder */
	/* XXX possible pmem2_init placeholder */
}

/*
 * libpmem2_fini -- libpmem2 cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmem2_fini(void)
{
	/* XXX LOG(3, NULL); placeholder */

	out_fini();
}

/*
 * pmem2_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmem2_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmem2_errormsg -- return last error message
 */
const char *
pmem2_errormsg(void)
{
	return pmem2_errormsgU();
}
#else
/*
 * pmem2_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmem2_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
