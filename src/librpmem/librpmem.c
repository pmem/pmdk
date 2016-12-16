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
 * librpmem.c -- entry points for librpmem
 */

#include <stdio.h>
#include <stdint.h>

#include "librpmem.h"

#include "rpmem.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "util.h"
#include "out.h"

#ifdef HAS_IBVERBS
#include <infiniband/verbs.h>
#endif

extern int Rpmem_fork_fail;

/*
 * librpmem_init -- load-time initialization for librpmem
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
librpmem_init(void)
{
	util_init();
	out_init(RPMEM_LOG_PREFIX, RPMEM_LOG_LEVEL_VAR, RPMEM_LOG_FILE_VAR,
			RPMEM_MAJOR_VERSION, RPMEM_MINOR_VERSION);
	LOG(3, NULL);
	rpmem_util_cmds_init();

#ifdef RPMEM_TIMESTAMPS
	rpmem_timer_init();
#endif
#ifdef HAS_IBVERBS
	Rpmem_fork_fail = ibv_fork_init();
	if (Rpmem_fork_fail)
		RPMEM_LOG(ERR, "Initialization libibverbs to support "
			"fork() failed. See librpmem(3) for details.");
#endif
}

/*
 * librpmem_fini -- librpmem cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
librpmem_fini(void)
{
	LOG(3, NULL);

#ifdef RPMEM_TIMESTAMPS
	rpmem_timer_fini();
#endif
	rpmem_util_cmds_fini();
	out_fini();
}

/*
 * rpmem_check_version -- see if library meets application version requirements
 */
const char *
rpmem_check_version(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != RPMEM_MAJOR_VERSION) {
		ERR("librpmem major version mismatch (need %u, found %u)",
			major_required, RPMEM_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > RPMEM_MINOR_VERSION) {
		ERR("librpmem minor version mismatch (need %u, found %u)",
			minor_required, RPMEM_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

/*
 * rpmem_errormsg -- return the last error message
 */
const char *
rpmem_errormsg(void)
{
	return out_get_errormsg();
}
