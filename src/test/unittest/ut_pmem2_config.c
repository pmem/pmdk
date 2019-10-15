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
 * ut_pmem2_config.h -- utility helper functions for libpmem2 config tests
 */

#include <libpmem2.h>
#include "unittest.h"
#include "ut_pmem2_config.h"
#include "ut_pmem2_utils.h"
/*
 * ut_pmem2_config_new -- allocates cfg (cannot fail)
 */
void
ut_pmem2_config_new(const char *file, int line, const char *func,
	struct pmem2_config **cfg)
{
	int ret = pmem2_config_new(cfg);
	ut_pmem2_expect_return(file, line, func, ret, 0);

	UT_ASSERTne(*cfg, NULL);
}

/*
 * ut_pmem2_config_set_fd -- sets fd (cannot fail)
 */
void
ut_pmem2_config_set_fd(const char *file, int line, const char *func,
	struct pmem2_config *cfg, int fd)
{
	int ret = pmem2_config_set_fd(cfg, fd);
	ut_pmem2_expect_return(file, line, func, ret, 0);
}

/*
 * ut_pmem2_config_delete -- deallocates cfg (cannot fail)
 */
void
ut_pmem2_config_delete(const char *file, int line, const char *func,
	struct pmem2_config **cfg)
{
	int ret = pmem2_config_delete(cfg);
	ut_pmem2_expect_return(file, line, func, ret, 0);

	UT_ASSERTeq(*cfg, NULL);
}
