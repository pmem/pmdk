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
 * rpmem_common_log.h -- common log macros for librpmem and rpmemd
 */

#if defined(RPMEMC_LOG_RPMEM) && defined(RPMEMC_LOG_RPMEMD)

#error Both RPMEMC_LOG_RPMEM and RPMEMC_LOG_RPMEMD defined

#elif !defined(RPMEMC_LOG_RPMEM) && !defined(RPMEMC_LOG_RPMEMD)

#define RPMEMC_LOG(level, fmt, args...) do {} while (0)
#define RPMEMC_DBG(level, fmt, args...) do {} while (0)
#define RPMEMC_FATAL(fmt, args...)	do {} while (0)
#define RPMEMC_ASSERT(cond)		do {} while (0)

#elif defined(RPMEMC_LOG_RPMEM)

#include "out.h"
#include "util.h"
#include "rpmem_util.h"

#define RPMEMC_LOG(level, fmt, args...) RPMEM_LOG(level, fmt, ## args)
#define RPMEMC_DBG(level, fmt, args...) RPMEM_DBG(fmt, ## args)
#define RPMEMC_FATAL(fmt, args...)	RPMEM_FATAL(fmt, ## args)
#define RPMEMC_ASSERT(cond)		RPMEM_ASSERT(cond)

#else

#include "rpmemd_log.h"

#define RPMEMC_LOG(level, fmt, args...) RPMEMD_LOG(level, fmt, ## args)
#define RPMEMC_DBG(level, fmt, args...) RPMEMD_DBG(fmt, ## args)
#define RPMEMC_FATAL(fmt, args...)	RPMEMD_FATAL(fmt, ## args)
#define RPMEMC_ASSERT(cond)		RPMEMD_ASSERT(cond)

#endif
