#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023, Intel Corporation

import json
from typing import Dict, List

# https://peps.python.org/pep-0613/ requires Python >= 3.10
# from typing import TypeAlias

# Calls: TypeAlias = dict[str, list[str]] # for Python >= 3.10
Calls = Dict[str, List[str]] # for Python < 3.10

API = [
        'libpmemobj_init',
        'pmem_init',
        'pmemobj_alloc_usable_size',
        'pmemobj_alloc',
        'pmemobj_cancel',
        'pmemobj_check_version',
        'pmemobj_check',
        'pmemobj_close',
        'pmemobj_cond_broadcast',
        'pmemobj_cond_signal',
        'pmemobj_cond_wait',
        'pmemobj_cond_zero',
        'pmemobj_create',
        'pmemobj_ctl_exec',
        'pmemobj_ctl_get',
        'pmemobj_ctl_set',
        'pmemobj_defer_free',
        'pmemobj_defrag',
        'pmemobj_direct',
        'pmemobj_drain',
        'pmemobj_errormsg',
        'pmemobj_first',
        'pmemobj_flush',
        'pmemobj_free',
        'pmemobj_get_user_data',
        'pmemobj_list_insert',
        'pmemobj_list_move',
        'pmemobj_list_remove',
        'pmemobj_memcpy_persist',
        'pmemobj_memcpy',
        'pmemobj_memmove',
        'pmemobj_memset_persist',
        'pmemobj_memset',
        'pmemobj_mutex_timedlock',
        'pmemobj_mutex_trylock',
        'pmemobj_mutex_zero',
        'pmemobj_next',
        'pmemobj_oid',
        'pmemobj_open',
        'pmemobj_persist',
        'pmemobj_publish',
        'pmemobj_realloc',
        'pmemobj_reserve',
        'pmemobj_root_size',
        'pmemobj_root',
        'pmemobj_rwlock_rdlock',
        'pmemobj_rwlock_timedrdlock',
        'pmemobj_rwlock_timedwrlock',
        'pmemobj_rwlock_tryrdlock',
        'pmemobj_rwlock_trywrlock',
        'pmemobj_rwlock_zero',
        'pmemobj_set_funcs',
        'pmemobj_set_user_data',
        'pmemobj_set_value',
        'pmemobj_strdup',
        'pmemobj_tx_abort',
        'pmemobj_tx_add_range_direct',
        'pmemobj_tx_add_range',
        'pmemobj_tx_begin',
        'pmemobj_tx_commit',
        'pmemobj_tx_end',
        'pmemobj_tx_errno',
        'pmemobj_tx_free',
        'pmemobj_tx_get_failure_behavior',
        'pmemobj_tx_get_user_data',
        'pmemobj_tx_lock',
        'pmemobj_tx_log_append_buffer',
        'pmemobj_tx_log_auto_alloc',
        'pmemobj_tx_log_intents_max_size',
        'pmemobj_tx_log_snapshots_max_size',
        'pmemobj_tx_process',
        'pmemobj_tx_publish',
        'pmemobj_tx_realloc',
        'pmemobj_tx_set_failure_behavior',
        'pmemobj_tx_set_user_data',
        'pmemobj_tx_stage',
        'pmemobj_tx_strdup',
        'pmemobj_tx_wcsdup',
        'pmemobj_tx_xadd_range_direct',
        'pmemobj_tx_xadd_range',
        'pmemobj_tx_xalloc',
        'pmemobj_tx_xlock',
        'pmemobj_tx_xlog_append_buffer',
        'pmemobj_tx_xstrdup',
        'pmemobj_tx_xwcsdup',
        'pmemobj_tx_zalloc',
        'pmemobj_tx_zrealloc',
        'pmemobj_type_num',
        'pmemobj_volatile',
        'pmemobj_xreserve',
        'pmemobj_tx_alloc',
        'pmemobj_wcsdup',
        'pmemobj_xalloc',
        'pmemobj_zalloc',
        'pmemobj_zrealloc',
        "pmemobj_xflush",
        "pmemobj_xpersist",
]

DEAD_END = [
        'prealloc'
]

def dict_extend(dict_, key, values):
        if key not in dict_.keys():
                dict_[key] = values
        else:
                dict_[key].extend(values)
        return dict_

def inlines(calls):
        calls['core_init'] = ['util_init', 'out_init']
        calls['core_fini'] = ['out_fini']
        calls['ERR'] = ['out_err']
        calls['Print'] = ['out_print_func']
        calls['common_init'] = ['core_init', 'util_mmap_init']
        calls['common_fini'] = ['util_mmap_fini', 'core_fini']
        calls['Last_errormsg_key_alloc'] = ['_Last_errormsg_key_alloc']
        calls['_Last_errormsg_key_alloc'] = ['os_once', 'os_tls_key_create']
        calls['flush_empty'] = ['flush_empty_nolog']

        calls = dict_extend(calls, 'libpmemobj_init', ['common_init'])
        calls = dict_extend(calls, 'out_common', ['out_snprintf'])
        calls = dict_extend(calls, 'run_vg_init', ['run_iterate_used'])

        calls = dict_extend(calls, 'palloc_heap_action_on_unlock', ['palloc_reservation_clear'])
        calls = dict_extend(calls, 'palloc_heap_action_on_cancel', ['palloc_reservation_clear'])

        calls = dict_extend(calls, 'util_uuid_generate', ['util_uuid_from_string'])

        return calls

def function_pointers(calls):
        # block_container_ops
        insert_all = ['container_ravl_insert_block', 'container_seglists_insert_block']
        get_rm_exact_all = ['container_ravl_get_rm_block_exact']
        get_rm_bestfit_all = ['container_ravl_get_rm_block_bestfit', 'container_seglists_get_rm_block_bestfit']
        is_empty_all = ['container_ravl_is_empty', 'container_seglists_is_empty']
        rm_all_all = ['container_ravl_rm_all', 'container_seglists_rm_all']
        destroy_all = ['container_ravl_destroy', 'container_seglists_destroy']

        calls = dict_extend(calls, 'bucket_insert_block', insert_all)

        calls = dict_extend(calls, 'bucket_fini', destroy_all)

        # memory_block_ops
        block_size_all = ['huge_block_size', 'run_block_size']
        prep_hdr_all = ['huge_prep_operation_hdr', 'run_prep_operation_hdr']
        get_lock_all = ['huge_get_lock', 'run_get_lock']
        get_state_all = ['huge_get_state', 'run_get_state']
        get_user_data_all = ['block_get_user_data']
        get_real_data_all = ['huge_get_real_data', 'run_get_real_data']
        get_user_size_all = ['block_get_user_size']
        get_real_size_all = ['block_get_real_size']
        write_header_all = ['block_write_header']
        invalidate_all = ['block_invalidate']
        ensure_header_type_all = ['huge_ensure_header_type', 'run_ensure_header_type']
        reinit_header_all = ['block_reinit_header']
        vg_init_all = ['huge_vg_init', 'run_vg_init']
        get_extra_all = ['block_get_extra']
        get_flags_all = ['block_get_flags']
        iterate_free_all = ['huge_iterate_free', 'run_iterate_free']
        iterate_used_all = ['huge_iterate_used', 'run_iterate_used']
        reinit_chunk_all = ['huge_reinit_chunk', 'run_reinit_chunk']
        calc_free_all = ['run_calc_free']
        get_bitmap_all = ['run_get_bitmap']
        fill_pct_all = ['huge_fill_pct', 'run_fill_pct']

        calls = dict_extend(calls, 'heap_free_chunk_reuse', prep_hdr_all)
        calls = dict_extend(calls, 'palloc_heap_action_exec', prep_hdr_all)

        calls = dict_extend(calls, 'alloc_prep_block', write_header_all)

        calls = dict_extend(calls, 'palloc_heap_action_on_cancel', invalidate_all)

        calls = dict_extend(calls, 'heap_get_bestfit_block', ensure_header_type_all)

        calls = dict_extend(calls, 'palloc_vg_register_alloc', reinit_header_all)

        calls = dict_extend(calls, 'heap_vg_open', vg_init_all)

        calls = dict_extend(calls, 'bucket_attach_run', iterate_free_all)

        calls = dict_extend(calls, 'heap_zone_foreach_object', iterate_used_all)

        calls = dict_extend(calls, 'recycler_element_new', calc_free_all)

        # memblock_header_ops
        get_size_all = ['memblock_header_legacy_get_size', 'memblock_header_compact_get_size', 'memblock_header_none_get_size']
        get_extra_all = ['memblock_header_legacy_get_extra', 'memblock_header_compact_get_extra', 'memblock_header_none_get_extra']
        get_flags_all = ['memblock_header_legacy_get_flags', 'memblock_header_compact_get_flags', 'memblock_header_none_get_flags']
        write_all = ['memblock_header_legacy_write', 'memblock_header_compact_write', 'memblock_header_none_write']
        invalidate_all = ['memblock_header_legacy_invalidate', 'memblock_header_compact_invalidate', 'memblock_header_none_invalidate']
        reinit_all = ['memblock_header_legacy_reinit', 'memblock_header_compact_reinit', 'memblock_header_none_reinit']

        calls = dict_extend(calls, 'block_write_header', write_all)
        calls = dict_extend(calls, 'block_invalidate', invalidate_all)
        calls = dict_extend(calls, 'block_reinit_header', reinit_all)

        # action_funcs
        exec_all = ['palloc_heap_action_exec', 'palloc_mem_action_exec']
        on_cancel_all = ['palloc_heap_action_on_cancel', 'palloc_mem_action_noop']
        on_process_all = ['palloc_heap_action_on_process', 'palloc_mem_action_noop']
        on_unlock_all = ['palloc_heap_action_on_unlock', 'palloc_mem_action_noop']

        calls = dict_extend(calls, 'palloc_cancel', on_cancel_all)

        calls = dict_extend(calls, 'palloc_exec_actions', on_process_all + on_unlock_all)

        # DAOS used CTLs
        # pmemobj_ctl_get("stats.heap.curr_allocated")
        # pmemobj_ctl_get("stats.heap.run_allocated")
        # pmemobj_ctl_get("stats.heap.run_active")
        get_all = ['ctl__persistent_curr_allocated_read', 'ctl__transient_run_allocated_read', 'ctl__transient_run_active_read']
        calls = dict_extend(calls, 'ctl_exec_query_read', get_all)

        # pmemobj_ctl_set("heap.arenas_assignment_type")
        # pmemobj_ctl_set("heap.alloc_class.new.desc", &pmemslab);
        # pmemobj_ctl_set("stats.enabled", &enabled);
        # pmemobj_ctl_set("stats.enabled", &enabled);
        set_all = ['ctl__arenas_assignment_type_write', 'ctl__desc_write', 'ctl__enabled_write']
        calls = dict_extend(calls, 'ctl_exec_query_write', set_all)

        calls = dict_extend(calls, 'ctl_query', ['ctl_exec_query_read', 'ctl_exec_query_write'])
        
        # pmem_ops
        persist_all = ['obj_rep_persist', 'obj_norep_persist']
        flush_all = ['obj_rep_flush', 'obj_norep_flush', 'operation_transient_clean', '']
        drain_all = ['obj_rep_drain', 'obj_norep_drain', 'operation_transient_drain']
        memcpy_all = ['obj_rep_memcpy', 'obj_norep_memcpy', 'operation_transient_memcpy']
        memmove_all = ['obj_rep_memmove', 'obj_norep_memmove']
        memset_all = ['obj_rep_memset', 'obj_norep_memset']

        calls = dict_extend(calls, 'pmemops_xpersist', persist_all)
        calls = dict_extend(calls, 'pmemops_xflush', persist_all)
        calls = dict_extend(calls, 'pmemops_drain', drain_all)
        calls = dict_extend(calls, 'pmemops_memcpy', memcpy_all)
        calls = dict_extend(calls, 'pmemops_memmove', memmove_all)
        calls = dict_extend(calls, 'pmemops_memset', memset_all)
        calls = dict_extend(calls, 'ulog_entry_apply', persist_all + flush_all)

        # per-replica functions
        persist_local_all = ['pmem_persist', 'obj_msync_nofail']
        flush_local_all = ['pmem_flush', 'obj_msync_nofail']
        drain_local_all = ['pmem_drain', 'obj_drain_empty']
        memcpy_local_all = ['pmem_memcpy', 'obj_nopmem_memcpy']
        memmove_local_all = ['pmem_memmove', 'obj_nopmem_memmove']
        memset_local_all = ['pmem_memset', 'obj_nopmem_memset']

        calls = dict_extend(calls, 'obj_norep_persist', persist_local_all)
        calls = dict_extend(calls, 'obj_rep_persist', persist_local_all)

        calls = dict_extend(calls, 'obj_norep_flush', flush_local_all)
        calls = dict_extend(calls, 'obj_rep_flush', flush_local_all)

        calls = dict_extend(calls, 'obj_norep_drain', drain_local_all)
        calls = dict_extend(calls, 'obj_rep_drain', drain_local_all)

        calls = dict_extend(calls, 'obj_norep_memcpy', memcpy_local_all)
        calls = dict_extend(calls, 'obj_rep_memcpy', memcpy_local_all)
        calls = dict_extend(calls, 'obj_rep_flush', memcpy_local_all)
        calls = dict_extend(calls, 'obj_replicas_check_basic', memcpy_local_all)

        calls = dict_extend(calls, 'obj_norep_memmove', memmove_local_all)
        calls = dict_extend(calls, 'obj_rep_memmove', memmove_local_all)

        calls = dict_extend(calls, 'obj_norep_memset', memset_local_all)
        calls = dict_extend(calls, 'obj_rep_memset', memset_local_all)

        return calls

def main():
        with open("white_list.json", 'r') as file:
                white_list = json.load(file)
        extra_calls = inlines({})
        extra_calls = function_pointers(extra_calls)
        config = {
                'filter': 'libpmemobj',
                'api': API,
                'dead_end': DEAD_END,
                'extra_calls': extra_calls,
                'white_list': white_list
        }
        with open("config.json", "w") as outfile: 
                json.dump(config, outfile, indent = 4)

if __name__ == '__main__':
        main()
