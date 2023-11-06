#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation
#
#
# Generate, based on cflow tool the complete calls graph for
# all libpmem and libpmemobj API cals.
#

PMDK_API_CALLS=" pmemobj_alloc  \
	pmemobj_cancel  \
	pmemobj_close  \
	pmemobj_create  \
	pmemobj_ctl_get  \
	pmemobj_ctl_set  \
	pmemobj_defer_free  \
	pmemobj_direct  \
	pmemobj_errormsg  \
	pmemobj_flush  \
	pmemobj_free  \
	pmemobj_memcpy_persist  \
	pmemobj_open  \
	pmemobj_reserve  \
	pmemobj_root  \
	pmemobj_tx_abort  \
	pmemobj_tx_add_range  \
	pmemobj_tx_add_range_direct  \
	pmemobj_tx_begin  \
	pmemobj_tx_commit  \
	pmemobj_tx_end  \
	pmemobj_tx_free  \
	pmemobj_tx_publish  \
	pmemobj_tx_stage  \
	pmemobjtx_xadd_range  \
	pmemobjtx_xalloc  \
	pmemobj_check_version  \
	pmemobj_set_funcs  \
	pmemobj_errormsg  \
	pmemobj_create  \
	pmemobj_open  \
	pmemobj_close  \
	pmemobj_check  \
	pmemobj_ctl_exec  \
	pmemobj_ctl_get  \
	pmemobj_ctl_set  \
	pmemobj_mutex_zero  \
	pmemobj_mutex_lock  \
	pmemobj_mutex_timedlock  \
	pmemobj_mutex_trylock  \
	pmemobj_mutex_unlock  \
	pmemobj_rwlock_zero  \
	pmemobj_rwlock_rdlock  \
	pmemobj_rwlock_wrlock  \
	pmemobj_rwlock_timedrdlock  \
	pmemobj_rwlock_timedwrlock  \
	pmemobj_rwlock_tryrdlock  \
	pmemobj_rwlock_trywrlock  \
	pmemobj_rwlock_unlock  \
	pmemobj_cond_zero  \
	pmemobj_cond_broadcast  \
	pmemobj_cond_signal  \
	pmemobj_cond_timedwait  \
	pmemobj_cond_wait  \
	pmemobj_pool_by_oid  \
	pmemobj_pool_by_ptr  \
	pmemobj_oid  \
	pmemobj_alloc  \
	pmemobj_xalloc  \
	pmemobj_zalloc  \
	pmemobj_realloc  \
	pmemobj_zrealloc  \
	pmemobj_strdup  \
	pmemobj_wcsdup  \
	pmemobj_free  \
	pmemobj_alloc_usable_size  \
	pmemobj_type_num  \
	pmemobj_root  \
	pmemobj_root_construct  \
	pmemobj_root_size  \
	pmemobj_first  \
	pmemobj_next  \
	pmemobj_list_insert  \
	pmemobj_list_insert_new  \
	pmemobj_list_remove  \
	pmemobj_list_move  \
	pmemobj_tx_begin  \
	pmemobj_tx_stage  \
	pmemobj_tx_abort  \
	pmemobj_tx_commit  \
	pmemobj_tx_end  \
	pmemobj_tx_errno  \
	pmemobj_tx_process  \
	pmemobj_tx_add_range  \
	pmemobj_tx_add_range_direct  \
	pmemobj_tx_xadd_range  \
	pmemobj_tx_xadd_range_direct  \
	pmemobj_tx_alloc  \
	pmemobj_tx_xalloc  \
	pmemobj_tx_zalloc  \
	pmemobj_tx_realloc  \
	pmemobj_tx_zrealloc  \
	pmemobj_tx_strdup  \
	pmemobj_tx_xstrdup  \
	pmemobj_tx_wcsdup  \
	pmemobj_tx_xwcsdup  \
	pmemobj_tx_free  \
	pmemobj_tx_xfree  \
	pmemobj_tx_lock  \
	pmemobj_tx_xlock  \
	pmemobj_tx_log_append_buffer  \
	pmemobj_tx_xlog_append_buffer  \
	pmemobj_tx_log_auto_alloc  \
	pmemobj_tx_log_snapshots_max_size  \
	pmemobj_tx_log_intents_max_size  \
	pmemobj_tx_set_user_data  \
	pmemobj_tx_get_user_data  \
	pmemobj_tx_set_failure_behavior  \
	pmemobj_tx_get_failure_behavior  \
	pmemobj_memcpy  \
	pmemobj_memcpy_persist  \
	pmemobj_memmove  \
	pmemobj_memset  \
	pmemobj_memset_persist  \
	pmemobj_persist  \
	pmemobj_flush  \
	pmemobj_drain  \
	pmemobj_xpersist  \
	pmemobj_xflush  \
	pmemobj_direct  \
	pmemobj_volatile  \
	pmemobj_reserve  \
	pmemobj_xreserve  \
	pmemobj_defer_free  \
	pmemobj_set_value  \
	pmemobj_publish  \
	pmemobj_tx_publish  \
	pmemobj_tx_xpublish  \
	pmemobj_cancel  \
	pmemobj_set_user_data  \
	pmemobj_get_user_data  \
	pmemobj_defrag  \
	pmem_map_file  \
	pmem_unmap  \
	pmem_is_pmem  \
	pmem_persist  \
	pmem_msync  \
	pmem_has_auto_flush  \
	pmem_deep_persist  \
	pmem_flush  \
	pmem_deep_flush  \
	pmem_deep_drain  \
	pmem_drain  \
	pmem_has_hw_drain  \
	pmem_check_version  \
	pmem_errormsg  \
	pmem_memmove_persist  \
	pmem_memcpy_persist  \
	pmem_memset_persist  \
	pmem_memmove_nodrain  \
	pmem_memcpy_nodrain  \
	pmem_memset_nodrain  \
	pmem_memmove  \
	pmem_memcpy  \
	pmem_memset  \
	fault_injection  \
\
"

STARTS=
for start in $PMDK_API_CALLS; do
	STARTS="$STARTS --start $start"
done

SOURCES=
for ss in `find . -name *.[ch] | grep -v -e '_other.c' -e '_none.c' -e /tools/ -e /test/ -e /aarch64/ -e /examples/ -e /ppc64/  -e /riscv64/ -e '/loongarch64/' -e '/libpmempool/' -e'/libpmem2/'`; do
	SOURCES="$SOURCES $ss"
done

cflow -o $(dirname "$0")/cflow.txt  \
	--symbol __inline:=inline  \
	--symbol __inline__:=inline  \
	--symbol __const__:=const  \
	--symbol __const:=const  \
	--symbol __restrict:=restrict  \
	--symbol __extension__:qualifier  \
	--symbol __attribute__:wrapper  \
	--symbol __asm__:wrapper  \
	--symbol __nonnull:wrapper  \
	--symbol __wur:wrapper  \
	--preprocess='gcc -E -I. -I.. -I./src/common -I./src/core -I./src/libpmemobj -I./src/libpmem -I./src/libpmem2 -I./src/include -I./src/libpmem2/x86_64 -std=gnu99 -Wall -Wmissing-prototypes -Wpointer-arith -Wsign-conversion -Wsign-compare -Wunused-parameter -fstack-usage -Wconversion -Wmissing-field-initializers -Wfloat-equal -Wswitch-default -Wcast-function-type -DSTRINGOP_TRUNCATION_SUPPORTED -O2 -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 -std=gnu99 -fno-common -pthread -DSRCVERSION=\"2.0.0+git53.g7f8cd6114\" -fno-lto -DSDS_ENABLED -DNDCTL_ENABLED=1 -fstack-usage'  \
	$STARTS $IGNORE_STR $SOURCES 2> $(dirname "$0")/cflow.err
