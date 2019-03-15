/*
 * Copyright 2015-2019, Intel Corporation
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
 * valgrind_internal.h -- internal definitions for valgrind macros
 */

#ifndef PMDK_VALGRIND_INTERNAL_H
#define PMDK_VALGRIND_INTERNAL_H 1

#if !defined(_WIN32) && !defined(__FreeBSD__)
#ifndef VALGRIND_ENABLED
#define VALGRIND_ENABLED 1
#endif
#endif

#if VALGRIND_ENABLED
#define VG_PMEMCHECK_ENABLED 1
#define VG_HELGRIND_ENABLED 1
#define VG_MEMCHECK_ENABLED 1
#define VG_DRD_ENABLED 1
#endif

#if VG_PMEMCHECK_ENABLED || VG_HELGRIND_ENABLED || VG_MEMCHECK_ENABLED || \
	VG_DRD_ENABLED
#define ANY_VG_TOOL_ENABLED 1
#else
#define ANY_VG_TOOL_ENABLED 0
#endif

#if ANY_VG_TOOL_ENABLED
extern unsigned _On_valgrind;
#define On_valgrind __builtin_expect(_On_valgrind, 0)
#include "valgrind/valgrind.h"
#else
#define On_valgrind (0)
#endif

#if VG_HELGRIND_ENABLED
#include "valgrind/helgrind.h"
#endif

#if VG_DRD_ENABLED
#include "valgrind/drd.h"
#endif

#if VG_HELGRIND_ENABLED || VG_DRD_ENABLED

#define VALGRIND_ANNOTATE_HAPPENS_BEFORE(obj) do {\
	if (On_valgrind) \
		ANNOTATE_HAPPENS_BEFORE((obj));\
} while (0)

#define VALGRIND_ANNOTATE_HAPPENS_AFTER(obj) do {\
	if (On_valgrind) \
		ANNOTATE_HAPPENS_AFTER((obj));\
} while (0)

#define VALGRIND_ANNOTATE_NEW_MEMORY(addr, size) do {\
	if (On_valgrind) \
		ANNOTATE_NEW_MEMORY((addr), (size));\
} while (0)

#define VALGRIND_ANNOTATE_IGNORE_READS_BEGIN() do {\
	if (On_valgrind) \
	ANNOTATE_IGNORE_READS_BEGIN();\
} while (0)

#define VALGRIND_ANNOTATE_IGNORE_READS_END() do {\
	if (On_valgrind) \
	ANNOTATE_IGNORE_READS_END();\
} while (0)

#define VALGRIND_ANNOTATE_IGNORE_WRITES_BEGIN() do {\
	if (On_valgrind) \
	ANNOTATE_IGNORE_WRITES_BEGIN();\
} while (0)

#define VALGRIND_ANNOTATE_IGNORE_WRITES_END() do {\
	if (On_valgrind) \
	ANNOTATE_IGNORE_WRITES_END();\
} while (0)

/* Supported by both helgrind and drd. */
#define VALGRIND_HG_DRD_DISABLE_CHECKING(addr, size) do {\
	if (On_valgrind) \
		VALGRIND_HG_DISABLE_CHECKING((addr), (size));\
} while (0)

#else

#define VALGRIND_ANNOTATE_HAPPENS_BEFORE(obj) do { (void)(obj); } while (0)

#define VALGRIND_ANNOTATE_HAPPENS_AFTER(obj) do { (void)(obj); } while (0)

#define VALGRIND_ANNOTATE_NEW_MEMORY(addr, size) do {\
	(void) (addr);\
	(void) (size);\
} while (0)

#define VALGRIND_ANNOTATE_IGNORE_READS_BEGIN() do {} while (0)

#define VALGRIND_ANNOTATE_IGNORE_READS_END() do {} while (0)

#define VALGRIND_ANNOTATE_IGNORE_WRITES_BEGIN() do {} while (0)

#define VALGRIND_ANNOTATE_IGNORE_WRITES_END() do {} while (0)

#define VALGRIND_HG_DRD_DISABLE_CHECKING(addr, size) do {\
	(void) (addr);\
	(void) (size);\
} while (0)

#endif

#if VG_PMEMCHECK_ENABLED

#include "valgrind/pmemcheck.h"

void pobj_emit_log(const char *func, int order);
void pmem_emit_log(const char *func, int order);
extern int _Pmreorder_emit;

#define Pmreorder_emit __builtin_expect(_Pmreorder_emit, 0)

#define VALGRIND_REGISTER_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REGISTER_PMEM_MAPPING((addr), (len));\
} while (0)

#define VALGRIND_REGISTER_PMEM_FILE(desc, base_addr, size, offset) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REGISTER_PMEM_FILE((desc), (base_addr), (size), \
		(offset));\
} while (0)

#define VALGRIND_REMOVE_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REMOVE_PMEM_MAPPING((addr), (len));\
} while (0)

#define VALGRIND_CHECK_IS_PMEM_MAPPING(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_CHECK_IS_PMEM_MAPPING((addr), (len));\
} while (0)

#define VALGRIND_PRINT_PMEM_MAPPINGS do {\
	if (On_valgrind)\
		VALGRIND_PMC_PRINT_PMEM_MAPPINGS;\
} while (0)

#define VALGRIND_DO_FLUSH(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_DO_FLUSH((addr), (len));\
} while (0)

#define VALGRIND_DO_FENCE do {\
	if (On_valgrind)\
		VALGRIND_PMC_DO_FENCE;\
} while (0)

#define VALGRIND_DO_PERSIST(addr, len) do {\
	if (On_valgrind) {\
		VALGRIND_PMC_DO_FLUSH((addr), (len));\
		VALGRIND_PMC_DO_FENCE;\
	}\
} while (0)

#define VALGRIND_SET_CLEAN(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_SET_CLEAN(addr, len);\
} while (0)

#define VALGRIND_WRITE_STATS do {\
	if (On_valgrind)\
		VALGRIND_PMC_WRITE_STATS;\
} while (0)

#define VALGRIND_EMIT_LOG(emit_log) do {\
	if (On_valgrind)\
		VALGRIND_PMC_EMIT_LOG((emit_log));\
} while (0)

#define VALGRIND_START_TX do {\
	if (On_valgrind)\
		VALGRIND_PMC_START_TX;\
} while (0)

#define VALGRIND_START_TX_N(txn) do {\
	if (On_valgrind)\
		VALGRIND_PMC_START_TX_N(txn);\
} while (0)

#define VALGRIND_END_TX do {\
	if (On_valgrind)\
		VALGRIND_PMC_END_TX;\
} while (0)

#define VALGRIND_END_TX_N(txn) do {\
	if (On_valgrind)\
		VALGRIND_PMC_END_TX_N(txn);\
} while (0)

#define VALGRIND_ADD_TO_TX(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_ADD_TO_TX(addr, len);\
} while (0)

#define VALGRIND_ADD_TO_TX_N(txn, addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_ADD_TO_TX_N(txn, addr, len);\
} while (0)

#define VALGRIND_REMOVE_FROM_TX(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REMOVE_FROM_TX(addr, len);\
} while (0)

#define VALGRIND_REMOVE_FROM_TX_N(txn, addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_REMOVE_FROM_TX_N(txn, addr, len);\
} while (0)

#define VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_PMC_ADD_TO_GLOBAL_TX_IGNORE(addr, len);\
} while (0)

/*
 * Logs library and function name with proper suffix
 * to pmemcheck store log file.
 */
#define PMEMOBJ_API_START()\
	if (Pmreorder_emit)\
		pobj_emit_log(__func__, 0);
#define PMEMOBJ_API_END()\
	if (Pmreorder_emit)\
		pobj_emit_log(__func__, 1);

#define PMEM_API_START()\
	if (Pmreorder_emit)\
		pmem_emit_log(__func__, 0);
#define PMEM_API_END()\
	if (Pmreorder_emit)\
		pmem_emit_log(__func__, 1);

#else

#define Pmreorder_emit (0)

#define VALGRIND_REGISTER_PMEM_MAPPING(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_REGISTER_PMEM_FILE(desc, base_addr, size, offset) do {\
	(void) (desc);\
	(void) (base_addr);\
	(void) (size);\
	(void) (offset);\
} while (0)

#define VALGRIND_REMOVE_PMEM_MAPPING(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_CHECK_IS_PMEM_MAPPING(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_PRINT_PMEM_MAPPINGS do {} while (0)

#define VALGRIND_DO_FLUSH(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_DO_FENCE do {} while (0)

#define VALGRIND_DO_PERSIST(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_SET_CLEAN(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_WRITE_STATS do {} while (0)

#define VALGRIND_EMIT_LOG(emit_log) do {\
	(void) (emit_log);\
} while (0)

#define VALGRIND_START_TX do {} while (0)

#define VALGRIND_START_TX_N(txn) do { (void) (txn); } while (0)

#define VALGRIND_END_TX do {} while (0)

#define VALGRIND_END_TX_N(txn) do {\
	(void) (txn);\
} while (0)

#define VALGRIND_ADD_TO_TX(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_ADD_TO_TX_N(txn, addr, len) do {\
	(void) (txn);\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_REMOVE_FROM_TX(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_REMOVE_FROM_TX_N(txn, addr, len) do {\
	(void) (txn);\
	(void) (addr);\
	(void) (len);\
} while (0)

#define VALGRIND_ADD_TO_GLOBAL_TX_IGNORE(addr, len) do {\
	(void) (addr);\
	(void) (len);\
} while (0)

#define PMEMOBJ_API_START() do {} while (0)

#define PMEMOBJ_API_END() do {} while (0)

#define PMEM_API_START() do {} while (0)

#define PMEM_API_END() do {} while (0)

#endif

#if VG_MEMCHECK_ENABLED

#include "valgrind/memcheck.h"

#define VALGRIND_DO_DISABLE_ERROR_REPORTING do {\
	if (On_valgrind)\
		VALGRIND_DISABLE_ERROR_REPORTING;\
} while (0)

#define VALGRIND_DO_ENABLE_ERROR_REPORTING do {\
	if (On_valgrind)\
		VALGRIND_ENABLE_ERROR_REPORTING;\
} while (0)

#define VALGRIND_DO_CREATE_MEMPOOL(heap, rzB, is_zeroed) do {\
	if (On_valgrind)\
		VALGRIND_CREATE_MEMPOOL(heap, rzB, is_zeroed);\
} while (0)

#define VALGRIND_DO_DESTROY_MEMPOOL(heap) do {\
	if (On_valgrind)\
		VALGRIND_DESTROY_MEMPOOL(heap);\
} while (0)

#define VALGRIND_DO_MEMPOOL_ALLOC(heap, addr, size) do {\
	if (On_valgrind)\
		VALGRIND_MEMPOOL_ALLOC(heap, addr, size);\
} while (0)

#define VALGRIND_DO_MEMPOOL_FREE(heap, addr) do {\
	if (On_valgrind)\
		VALGRIND_MEMPOOL_FREE(heap, addr);\
} while (0)

#define VALGRIND_DO_MEMPOOL_CHANGE(heap, addrA, addrB, size) do {\
	if (On_valgrind)\
		VALGRIND_MEMPOOL_CHANGE(heap, addrA, addrB, size);\
} while (0)

#define VALGRIND_DO_MAKE_MEM_DEFINED(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_MAKE_MEM_DEFINED(addr, len);\
} while (0)

#define VALGRIND_DO_MAKE_MEM_UNDEFINED(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_MAKE_MEM_UNDEFINED(addr, len);\
} while (0)

#define VALGRIND_DO_MAKE_MEM_NOACCESS(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_MAKE_MEM_NOACCESS(addr, len);\
} while (0)

#define VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len) do {\
	if (On_valgrind)\
		VALGRIND_CHECK_MEM_IS_ADDRESSABLE(addr, len);\
} while (0)

#else

#define VALGRIND_DO_DISABLE_ERROR_REPORTING do {} while (0)

#define VALGRIND_DO_ENABLE_ERROR_REPORTING do {} while (0)

#define VALGRIND_DO_CREATE_MEMPOOL(heap, rzB, is_zeroed)\
	do { (void) (heap); (void) (rzB); (void) (is_zeroed); } while (0)

#define VALGRIND_DO_DESTROY_MEMPOOL(heap)\
	do { (void) (heap); } while (0)

#define VALGRIND_DO_MEMPOOL_ALLOC(heap, addr, size)\
	do { (void) (heap); (void) (addr); (void) (size); } while (0)

#define VALGRIND_DO_MEMPOOL_FREE(heap, addr)\
	do { (void) (heap); (void) (addr); } while (0)

#define VALGRIND_DO_MEMPOOL_CHANGE(heap, addrA, addrB, size)\
	do {\
		(void) (heap); (void) (addrA); (void) (addrB); (void) (size);\
	} while (0)

#define VALGRIND_DO_MAKE_MEM_DEFINED(addr, len)\
	do { (void) (addr); (void) (len); } while (0)

#define VALGRIND_DO_MAKE_MEM_UNDEFINED(addr, len)\
	do { (void) (addr); (void) (len); } while (0)

#define VALGRIND_DO_MAKE_MEM_NOACCESS(addr, len)\
	do { (void) (addr); (void) (len); } while (0)

#define VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len)\
	do { (void) (addr); (void) (len); } while (0)

#endif

#endif
