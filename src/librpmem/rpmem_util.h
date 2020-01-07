// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * rpmem_util.h -- util functions for librpmem header file
 */

#ifndef RPMEM_UTIL_H
#define RPMEM_UTIL_H 1

#ifdef __cplusplus
extern "C" {
#endif

enum {
	LERR = 1,
	LWARN = 2,
	LNOTICE = 3,
	LINFO = 4,
	_LDBG = 10,
};

#define RPMEM_LOG(level, fmt, args...) LOG(L##level, fmt, ## args)
#define RPMEM_DBG(fmt, args...) LOG(_LDBG, fmt, ## args)
#define RPMEM_FATAL(fmt, args...) FATAL(fmt, ## args)
#define RPMEM_ASSERT(cond)	ASSERT(cond)

#define RPMEM_PERSIST_FLAGS_ALL		RPMEM_PERSIST_RELAXED
#define RPMEM_PERSIST_FLAGS_MASK	((unsigned)(~RPMEM_PERSIST_FLAGS_ALL))

#define RPMEM_FLUSH_FLAGS_ALL		RPMEM_FLUSH_RELAXED
#define RPMEM_FLUSH_FLAGS_MASK		((unsigned)(~RPMEM_FLUSH_FLAGS_ALL))

const char *rpmem_util_proto_errstr(enum rpmem_err err);
int rpmem_util_proto_errno(enum rpmem_err err);

void rpmem_util_cmds_init(void);
void rpmem_util_cmds_fini(void);
const char *rpmem_util_cmd_get(void);
void rpmem_util_get_env_max_nlanes(unsigned *max_nlanes);
void rpmem_util_get_env_wq_size(unsigned *wq_size);

#ifdef __cplusplus
}
#endif

#endif
