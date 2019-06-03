/*
 * Copyright 2016-2019, Intel Corporation
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
