#!/usr/bin/env python3

# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2023-2024, Intel Corporation

import json
from typing import Dict, List

# https://peps.python.org/pep-0613/ requires Python >= 3.10
# from typing import TypeAlias

# Calls: TypeAlias = dict[str, list[str]] # for Python >= 3.10
Calls = Dict[str, List[str]] # for Python < 3.10

def dict_extend(dict_, key, values):
        if key not in dict_.keys():
                dict_[key] = values
        else:
                dict_[key].extend(values)
        return dict_

def inlines(calls: Calls) -> Calls:
        # common
        calls['core_init'] = ['util_init', 'core_log_init', 'out_init']
        calls['core_fini'] = ['out_fini', 'core_log_fini', 'last_error_msg_fini']
        calls['common_init'] = ['core_init', 'util_mmap_init']
        calls['common_fini'] = ['util_mmap_fini', 'core_fini']
        calls['core_log_init'] = ['core_log_default_init', 'core_log_set_function']

        # libpmem
        calls['flush_empty'] = ['flush_empty_nolog']

        # libpmemobj
        calls['libpmemobj_init'] = ['common_init']
        calls['run_vg_init'] = ['run_iterate_used']
        calls['palloc_heap_action_on_unlock'] = ['palloc_reservation_clear']
        calls['palloc_heap_action_on_cancel'] = ['palloc_reservation_clear']
        calls['util_uuid_generate'] = ['util_uuid_from_string']

        return calls

def core_function_pointers(calls: Calls) -> Calls:
        calls['core_log_va'] = ['core_log_default_function']
        return calls

def pmem_function_pointers(calls: Calls) -> Calls:
        calls['pmem_drain'] = ['fence_empty', 'memory_barrier']

        flush_all = ['flush_empty', 'flush_clflush', 'flush_clflushopt', 'flush_clwb']
        calls['pmem_deep_flush'] = flush_all
        calls['pmem_flush'] = flush_all

        # '.static' suffix added to differentiate between libpmem API function and a static helper.
        memmove_nodrain_all = ['memmove_nodrain_libc', 'memmove_nodrain_generic', 'pmem2_memmove_nodrain', 'pmem2_memmove_nodrain_eadr']
        calls['pmem_memmove'] = memmove_nodrain_all
        calls['pmem_memcpy'] = memmove_nodrain_all
        calls['pmem_memmove_nodrain'] = memmove_nodrain_all
        calls['pmem_memcpy_nodrain'] = memmove_nodrain_all
        calls['pmem_memmove_persist'] = memmove_nodrain_all
        calls['pmem_memcpy_persist'] = memmove_nodrain_all

        memset_nodrain_all = ['memset_nodrain_libc', 'memset_nodrain_generic', 'pmem2_memset_nodrain', 'pmem2_memset_nodrain_eadr']
        calls['pmem_memset'] = memset_nodrain_all
        calls['pmem_memset_nodrain'] = memset_nodrain_all
        calls['pmem_memset_persist'] = memset_nodrain_all

        memmove_funcs = {
                't': {
                        func: [ f'memmove_mov_{trick}_{func}'
                                for trick in ['sse2', 'avx', 'avx512f']
                        ] for func in ['noflush', 'empty']
                },
                'nt': {
                        'empty': [ f'memmove_movnt_{trick}_empty_{drain}'
                                for trick in ['sse2', 'avx']
                                        for drain in ['wcbarrier', 'nobarrier']
                        ],
                        'flush': [ f'memmove_movnt_{trick}_{flush}_{drain}'
                                for trick in ['sse2', 'avx']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                                                for drain in ['wcbarrier', 'nobarrier']
                        ]
                }
        }
        memmove_funcs_extras = {
                't': {
                        'flush': [ f'memmove_mov_{trick}_{flush}'
                                for trick in ['sse2', 'avx', 'avx512f']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                        ]
                },
                'nt': {
                        'empty': [ f'memmove_movnt_{trick}_empty'
                                for trick in ['avx512f', 'movdir64b']
                        ],
                        'flush': [ f'memmove_movnt_{trick}_{flush}'
                                for trick in ['avx512f', 'movdir64b']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                        ]
                }
        }
        memmove_funcs['t']['flush'] = memmove_funcs_extras['t']['flush']
        memmove_funcs['nt']['empty'].extend(memmove_funcs_extras['nt']['empty'])
        memmove_funcs['nt']['flush'].extend(memmove_funcs_extras['nt']['flush'])

        calls['pmem2_memmove_nodrain'] = \
                            memmove_funcs['t']['noflush'] + \
                            memmove_funcs['nt']['flush'] + \
                            memmove_funcs['t']['flush']

        calls['pmem2_memmove_nodrain_eadr'] = \
                            memmove_funcs['t']['noflush'] + \
                            memmove_funcs['nt']['empty'] + \
                            memmove_funcs['t']['empty']

        memsetfuncs = {
                't': {
                        func: [ f'memset_mov_{trick}_{func}'
                                for trick in ['sse2', 'avx', 'avx512f']
                        ] for func in ['noflush', 'empty']
                },
                'nt': {
                        'empty': [ f'memset_movnt_{trick}_empty_{drain}'
                                for trick in ['sse2', 'avx']
                                        for drain in ['wcbarrier', 'nobarrier']
                        ],
                        'flush': [ f'memset_movnt_{trick}_{flush}_{drain}'
                                for trick in ['sse2', 'avx']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                                                for drain in ['wcbarrier', 'nobarrier']
                        ]
                }
        }
        memsetfuncs_extras = {
                't': {
                        'flush': [ f'memset_mov_{trick}_{flush}'
                                for trick in ['sse2', 'avx', 'avx512f']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                        ]
                },
                'nt': {
                        'empty': [ f'memset_movnt_{trick}_empty'
                                for trick in ['avx512f', 'movdir64b']
                        ],
                        'flush': [ f'memset_movnt_{trick}_{flush}'
                                for trick in ['avx512f', 'movdir64b']
                                        for flush in ['clflush', 'clflushopt', 'clwb']
                        ]
                }
        }
        memsetfuncs['t']['flush'] = memsetfuncs_extras['t']['flush']
        memsetfuncs['nt']['empty'].extend(memsetfuncs_extras['nt']['empty'])
        memsetfuncs['nt']['flush'].extend(memsetfuncs_extras['nt']['flush'])

        calls['pmem2_memset_nodrain'] = \
                            memsetfuncs['t']['noflush'] + \
                            memsetfuncs['nt']['flush'] + \
                            memsetfuncs['t']['flush']

        calls['pmem2_memset_nodrain_eadr'] = \
                            memsetfuncs['t']['noflush'] + \
                            memsetfuncs['nt']['empty'] + \
                            memsetfuncs['t']['empty']

        is_pmem_all = ['is_pmem_never', 'is_pmem_always', 'is_pmem_detect']

        calls = dict_extend(calls, 'pmem_is_pmem', is_pmem_all)

        return calls

def pmemobj_function_pointers(calls: Calls) -> Calls:
        # block_container_ops
        insert_all = ['container_ravl_insert_block', 'container_seglists_insert_block']
        get_rm_exact_all = ['container_ravl_get_rm_block_exact']
        get_rm_bestfit_all = ['container_ravl_get_rm_block_bestfit', 'container_seglists_get_rm_block_bestfit']
        is_empty_all = ['container_ravl_is_empty', 'container_seglists_is_empty']
        rm_all_all = ['container_ravl_rm_all', 'container_seglists_rm_all']
        destroy_all = ['container_ravl_destroy', 'container_seglists_destroy']

        calls = dict_extend(calls, 'bucket_insert_block', insert_all)

        calls = dict_extend(calls, 'bucket_remove_block', get_rm_exact_all)

        calls = dict_extend(calls, 'bucket_alloc_block', get_rm_bestfit_all)

        callers = [
                'bucket_attach_run',
                'bucket_detach_run'
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, rm_all_all)

        calls = dict_extend(calls, 'bucket_fini', destroy_all)

        compare_all = ['ravl_interval_compare']

        callers = [
                'ravl_emplace',
                'ravl_find'
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, compare_all)

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

        callers = [
                'memblock_header_none_get_size',
                'block_get_real_size',
                'memblock_from_offset_opt',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, block_size_all)

        callers = [
                'heap_free_chunk_reuse',
                'palloc_heap_action_exec',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, prep_hdr_all)

        callers = [
                'bucket_attach_run',
                'heap_run_into_free_chunk',
                'palloc_reservation_create',
                'palloc_defer_free_create',
                'palloc_defrag',
                'recycler_element_new',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, get_lock_all)

        callers = [
                'container_ravl_insert_block',
                'block_invalidate',
                'alloc_prep_block',
                'palloc_heap_action_on_cancel',
                'palloc_heap_action_on_process',
                'palloc_first',
                'palloc_next',
                'palloc_vg_register_alloc',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, get_user_data_all)

        callers = [
                'bucket_insert_block',
                'memblock_header_legacy_get_size',
                'memblock_header_compact_get_size',
                'memblock_header_legacy_get_extra',
                'memblock_header_compact_get_extra',
                'memblock_header_legacy_get_flags',
                'memblock_header_compact_get_flags',
                'memblock_header_legacy_write',
                'memblock_header_compact_write',
                'memblock_header_legacy_invalidate',
                'memblock_header_compact_invalidate',
                'memblock_header_legacy_reinit',
                'memblock_header_compact_reinit',
                'block_get_user_data',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, get_real_data_all)

        callers = [
                'block_invalidate',
                'alloc_prep_block',
                'palloc_operation',
                'palloc_defrag',
                'palloc_usable_size',
                'palloc_vg_register_alloc',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, get_user_size_all)

        calls = dict_extend(calls, 'alloc_prep_block', write_header_all)

        calls = dict_extend(calls, 'palloc_heap_action_on_cancel', invalidate_all)

        calls = dict_extend(calls, 'heap_get_bestfit_block', ensure_header_type_all)

        calls = dict_extend(calls, 'palloc_vg_register_alloc', reinit_header_all)

        calls = dict_extend(calls, 'heap_vg_open', vg_init_all)

        calls = dict_extend(calls, 'palloc_defrag', get_extra_all)
        calls = dict_extend(calls, 'palloc_extra', get_extra_all)

        calls = dict_extend(calls, 'palloc_defrag', get_flags_all)
        calls = dict_extend(calls, 'palloc_flags', get_flags_all)

        calls = dict_extend(calls, 'bucket_attach_run', iterate_free_all)

        calls = dict_extend(calls, 'heap_zone_foreach_object', iterate_used_all)

        calls = dict_extend(calls, 'heap_reclaim_zone_garbage', reinit_chunk_all)

        calls = dict_extend(calls, 'recycler_element_new', calc_free_all)

        calls = dict_extend(calls, 'palloc_defrag', fill_pct_all)

        # memblock_header_ops
        get_size_all = ['memblock_header_legacy_get_size', 'memblock_header_compact_get_size', 'memblock_header_none_get_size']
        get_extra_all = ['memblock_header_legacy_get_extra', 'memblock_header_compact_get_extra', 'memblock_header_none_get_extra']
        get_flags_all = ['memblock_header_legacy_get_flags', 'memblock_header_compact_get_flags', 'memblock_header_none_get_flags']
        write_all = ['memblock_header_legacy_write', 'memblock_header_compact_write', 'memblock_header_none_write']
        invalidate_all = ['memblock_header_legacy_invalidate', 'memblock_header_compact_invalidate', 'memblock_header_none_invalidate']
        reinit_all = ['memblock_header_legacy_reinit', 'memblock_header_compact_reinit', 'memblock_header_none_reinit']

        callers = [
                'block_get_real_size',
                'memblock_from_offset_opt',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, get_size_all)

        calls = dict_extend(calls, 'block_get_extra', get_extra_all)
        calls = dict_extend(calls, 'block_get_flags', get_flags_all)
        calls = dict_extend(calls, 'block_write_header', write_all)
        calls = dict_extend(calls, 'block_invalidate', invalidate_all)
        calls = dict_extend(calls, 'block_reinit_header', reinit_all)

        # action_funcs
        exec_all = ['palloc_heap_action_exec', 'palloc_mem_action_exec']
        on_cancel_all = ['palloc_heap_action_on_cancel', 'palloc_mem_action_noop']
        on_process_all = ['palloc_heap_action_on_process', 'palloc_mem_action_noop']
        on_unlock_all = ['palloc_heap_action_on_unlock', 'palloc_mem_action_noop']

        calls = dict_extend(calls, 'palloc_exec_actions', exec_all)

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

        callers = [
                'obj_norep_memcpy',
                'obj_rep_memcpy',
                'obj_rep_flush',
                'obj_replicas_check_basic',
        ]
        for caller in callers:
                calls = dict_extend(calls, caller, memcpy_local_all)

        calls = dict_extend(calls, 'obj_norep_memmove', memmove_local_all)
        calls = dict_extend(calls, 'obj_rep_memmove', memmove_local_all)

        calls = dict_extend(calls, 'obj_norep_memset', memset_local_all)
        calls = dict_extend(calls, 'obj_rep_memset', memset_local_all)

        return calls

def get_callees(calls):
        callees = []
        for _, v in calls.items():
                callees.extend(v)
        return list(set(callees))

# XXX
# The way how inlines() function is used shall be changed according to:
# https://github.com/pmem/pmdk/issues/6070
def main():
        extra_calls = core_function_pointers({})
        extra_calls = pmem_function_pointers(extra_calls)
        extra_calls = pmemobj_function_pointers(extra_calls)
        with open("extra_calls.json", "w") as outfile:
                json.dump(extra_calls, outfile, indent = 4)

        extra_calls = inlines(extra_calls)
        # All functions accessed via function pointers have to be provided
        # on top of regular API calls for cflow to process their call stacks.
        extra_entry_points = get_callees(extra_calls)
        extra_entry_points.sort()
        with open("extra_entry_points.txt", "w") as outfile:
                outfile.write("\n".join(extra_entry_points))

if __name__ == '__main__':
        main()
