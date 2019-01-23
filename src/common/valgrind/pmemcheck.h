/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#ifndef __PMEMCHECK_H
#define __PMEMCHECK_H


/* This file is for inclusion into client (your!) code.

   You can use these macros to manipulate and query memory permissions
   inside your own programs.

   See comment near the top of valgrind.h on how to use them.
*/

#include "valgrind.h"

/* !! ABIWARNING !! ABIWARNING !! ABIWARNING !! ABIWARNING !!
   This enum comprises an ABI exported by Valgrind to programs
   which use client requests.  DO NOT CHANGE THE ORDER OF THESE
   ENTRIES, NOR DELETE ANY -- add new ones at the end. */
typedef
   enum {
       VG_USERREQ__PMC_REGISTER_PMEM_MAPPING = VG_USERREQ_TOOL_BASE('P','C'),
       VG_USERREQ__PMC_REGISTER_PMEM_FILE,
       VG_USERREQ__PMC_REMOVE_PMEM_MAPPING,
       VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING,
       VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS,
       VG_USERREQ__PMC_DO_FLUSH,
       VG_USERREQ__PMC_DO_FENCE,
       VG_USERREQ__PMC_RESERVED1,  /* Do not use. */
       VG_USERREQ__PMC_WRITE_STATS,
       VG_USERREQ__PMC_RESERVED2,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED3,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED4,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED5,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED7,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED8,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED9,  /* Do not use. */
       VG_USERREQ__PMC_RESERVED10, /* Do not use. */
       VG_USERREQ__PMC_SET_CLEAN,
       /* transaction support */
       VG_USERREQ__PMC_START_TX,
       VG_USERREQ__PMC_START_TX_N,
       VG_USERREQ__PMC_END_TX,
       VG_USERREQ__PMC_END_TX_N,
       VG_USERREQ__PMC_ADD_TO_TX,
       VG_USERREQ__PMC_ADD_TO_TX_N,
       VG_USERREQ__PMC_REMOVE_FROM_TX,
       VG_USERREQ__PMC_REMOVE_FROM_TX_N,
       VG_USERREQ__PMC_ADD_THREAD_TO_TX_N,
       VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N,
       VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE,
       VG_USERREQ__PMC_RESERVED6,  /* Do not use. */
       VG_USERREQ__PMC_EMIT_LOG,
   } Vg_PMemCheckClientRequest;



/* Client-code macros to manipulate pmem mappings */

/** Register a persistent memory mapping region */
#define VALGRIND_PMC_REGISTER_PMEM_MAPPING(_qzz_addr, _qzz_len)             \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REGISTER_PMEM_MAPPING,          \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register a persistent memory file */
#define VALGRIND_PMC_REGISTER_PMEM_FILE(_qzz_desc, _qzz_addr_base,          \
                                        _qzz_size, _qzz_offset)             \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REGISTER_PMEM_FILE,             \
                            (_qzz_desc), (_qzz_addr_base), (_qzz_size),     \
                            (_qzz_offset), 0)

/** Remove a persistent memory mapping region */
#define VALGRIND_PMC_REMOVE_PMEM_MAPPING(_qzz_addr,_qzz_len)                \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REMOVE_PMEM_MAPPING,            \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Check if the given range is a registered persistent memory mapping */
#define VALGRIND_PMC_CHECK_IS_PMEM_MAPPING(_qzz_addr,_qzz_len)              \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_CHECK_IS_PMEM_MAPPING,          \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register an SFENCE */
#define VALGRIND_PMC_PRINT_PMEM_MAPPINGS                                    \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_PRINT_PMEM_MAPPINGS,    \
                                    0, 0, 0, 0, 0)

/** Register a CLFLUSH-like operation */
#define VALGRIND_PMC_DO_FLUSH(_qzz_addr,_qzz_len)                           \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_DO_FLUSH,                       \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Register an SFENCE */
#define VALGRIND_PMC_DO_FENCE                                               \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_DO_FENCE,               \
                                    0, 0, 0, 0, 0)

/** Write tool stats */
#define VALGRIND_PMC_WRITE_STATS                                            \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_WRITE_STATS,            \
                                    0, 0, 0, 0, 0)

/** Emit user log */
#define VALGRIND_PMC_EMIT_LOG(_qzz_emit_log)                                \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_EMIT_LOG,                       \
                            (_qzz_emit_log), 0, 0, 0, 0)

/** Set a region of persistent memory as clean */
#define VALGRIND_PMC_SET_CLEAN(_qzz_addr,_qzz_len)                          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_SET_CLEAN,                      \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Support for transactions */

/** Start an implicit persistent memory transaction */
#define VALGRIND_PMC_START_TX                                               \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_START_TX,               \
                                    0, 0, 0, 0, 0)

/** Start an explicit persistent memory transaction */
#define VALGRIND_PMC_START_TX_N(_qzz_txn)                                   \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_START_TX_N,                     \
                            (_qzz_txn), 0, 0, 0, 0)

/** End an implicit persistent memory transaction */
#define VALGRIND_PMC_END_TX                                                 \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_END_TX,                 \
                                    0, 0, 0, 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_END_TX_N(_qzz_txn)                                     \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_END_TX_N,                       \
                            (_qzz_txn), 0, 0, 0, 0)

/** Add a persistent memory region to the implicit transaction */
#define VALGRIND_PMC_ADD_TO_TX(_qzz_addr,_qzz_len)                          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_ADD_TO_TX,                      \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Add a persistent memory region to an explicit transaction */
#define VALGRIND_PMC_ADD_TO_TX_N(_qzz_txn,_qzz_addr,_qzz_len)               \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_ADD_TO_TX_N,                    \
                            (_qzz_txn), (_qzz_addr), (_qzz_len), 0, 0)

/** Remove a persistent memory region from the implicit transaction */
#define VALGRIND_PMC_REMOVE_FROM_TX(_qzz_addr,_qzz_len)                     \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REMOVE_FROM_TX,                 \
                            (_qzz_addr), (_qzz_len), 0, 0, 0)

/** Remove a persistent memory region from an explicit transaction */
#define VALGRIND_PMC_REMOVE_FROM_TX_N(_qzz_txn,_qzz_addr,_qzz_len)          \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REMOVE_FROM_TX_N,               \
                            (_qzz_txn), (_qzz_addr), (_qzz_len), 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_ADD_THREAD_TX_N(_qzz_txn)                              \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_ADD_THREAD_TO_TX_N,             \
                            (_qzz_txn), 0, 0, 0, 0)

/** End an explicit persistent memory transaction */
#define VALGRIND_PMC_REMOVE_THREAD_FROM_TX_N(_qzz_txn)                      \
    VALGRIND_DO_CLIENT_REQUEST_EXPR(0 /* default return */,                 \
                            VG_USERREQ__PMC_REMOVE_THREAD_FROM_TX_N,        \
                            (_qzz_txn), 0, 0, 0, 0)

/** Remove a persistent memory region from the implicit transaction */
#define VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(_qzz_addr,_qzz_len)            \
    VALGRIND_DO_CLIENT_REQUEST_STMT(VG_USERREQ__PMC_ADD_TO_GLOBAL_TX_IGNORE,\
                                    (_qzz_addr), (_qzz_len), 0, 0, 0)

#endif
