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
 * ut_pmem2.h -- utility helper functions for libpmem2 tests
 */

#include <libpmem2.h>
#include "unittest.h"

/*
 * ut_pmem2_alloc_cfg -- allocates cfg (cannot fail)
 */
static void
ut_pmem2_alloc_cfg(struct pmem2_config **cfg)
{
	if (pmem2_config_new(cfg))
		UT_FATAL("pmem2_config_new: %s", pmem2_errormsg());

	UT_ASSERTne(*cfg, NULL);
}

/*
 * ut_pmem2_set_fd -- sets fd (cannot fail)
 */
static void
ut_pmem2_set_fd(struct pmem2_config *cfg, int fd)
{
	if (pmem2_config_set_fd(cfg, fd))
		UT_FATAL("pmem2_config_set_fd: %s", pmem2_errormsg());
}

/*
 * ut_pmem2_delete_cfg -- deallocates cfg (cannot fail)
 */
static void
ut_pmem2_delete_cfg(struct pmem2_config **cfg)
{
	if (pmem2_config_delete(cfg))
		UT_FATAL("pmem2_config_delete: %s", pmem2_errormsg());

	UT_ASSERTeq(*cfg, NULL);
}
