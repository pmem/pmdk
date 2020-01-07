// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

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
