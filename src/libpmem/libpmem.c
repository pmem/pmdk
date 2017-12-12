/*
 * Copyright 2014-2017, Intel Corporation
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
 * libpmem.c -- pmem entry points for libpmem
 */

#include <stdio.h>
#include <stdint.h>

#include "libpmem.h"

#include "pmem.h"
#include "pmemcommon.h"

/*
 * libpmem_init -- load-time initialization for libpmem
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmem_init(void)
{
	common_init(PMEM_LOG_PREFIX, PMEM_LOG_LEVEL_VAR, PMEM_LOG_FILE_VAR,
			PMEM_MAJOR_VERSION, PMEM_MINOR_VERSION);
	LOG(3, NULL);
	pmem_init();
}

/*
 * libpmem_fini -- libpmem cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmem_fini(void)
{
	LOG(3, NULL);

	common_fini();
}

/*
 * pmem_check_versionU -- see if library meets application version requirements
 */
#ifndef _WIN32
static inline
#endif
const char *
pmem_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEM_MAJOR_VERSION) {
		ERR("libpmem major version mismatch (need %u, found %u)",
			major_required, PMEM_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEM_MINOR_VERSION) {
		ERR("libpmem minor version mismatch (need %u, found %u)",
			minor_required, PMEM_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

#ifndef _WIN32
/*
 * pmem_check_version -- see if library meets application version requirements
 */
const char *
pmem_check_version(unsigned major_required, unsigned minor_required)
{
	return pmem_check_versionU(major_required, minor_required);
}
#else
/*
 * pmem_check_versionW -- see if library meets application version requirements
 */
const wchar_t *
pmem_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmem_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmem_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmem_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmem_errormsg -- return last error message
 */
const char *
pmem_errormsg(void)
{
	return pmem_errormsgU();
}
#else
/*
 * pmem_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmem_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
