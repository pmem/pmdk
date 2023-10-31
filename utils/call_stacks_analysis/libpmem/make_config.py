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
        'pmem_map_file',
        'pmem_unmap',
        'pmem_memset',
        'pmem_memmove',
        'pmem_memcpy',
        'pmem_memset_persist',
        'pmem_memmove_persist',
        'pmem_memcpy_persist',
        'pmem_memcpy_nodrain',
        "pmem_deep_persist",
        "pmem_persist",
        "pmem_check_version",
        "libpmem_init",
        "libpmem_fini",
        "pmem_has_hw_drain",
        "pmem_has_auto_flush",
        "pmem_errormsg",
        "pmem_memset_persist",
        "pmem_memmove_persist"
]

def inlines(calls: Calls) -> Calls:
        calls['core_init'] = ['util_init', 'out_init']
        calls['core_fini'] = ['out_fini']
        calls['ERR'] = ['out_err']
        calls['Print'] = ['out_print_func']
        calls['common_init'] = ['core_init', 'util_mmap_init']
        calls['common_fini'] = ['util_mmap_fini', 'core_fini']
        calls['Last_errormsg_key_alloc'] = ['_Last_errormsg_key_alloc']
        calls['_Last_errormsg_key_alloc'] = ['os_once', 'os_tls_key_create']
        calls['flush_empty'] = ['flush_empty_nolog']
        return calls

def function_pointers(calls: Calls) -> Calls:
        calls['pmem_drain'] = ['fence_empty', 'memory_barrier']

        flush_all = ['flush_empty', 'flush_clflush', 'flush_clflushopt', 'flush_clwb']
        calls['pmem_deep_flush'] = flush_all
        calls['pmem_flush'] = flush_all

        # '.static' suffix added to differentiate between libpmem API function and a static helper.
        memmove_nodrain_all = ['memmove_nodrain_libc', 'memmove_nodrain_generic', 'pmem_memmove_nodrain.static', 'pmem_memmove_nodrain_eadr.static']
        calls['pmem_memmove'] = memmove_nodrain_all
        calls['pmem_memcpy'] = memmove_nodrain_all
        calls['pmem_memmove_nodrain'] = memmove_nodrain_all
        calls['pmem_memcpy_nodrain'] = memmove_nodrain_all
        calls['pmem_memmove_persist'] = memmove_nodrain_all
        calls['pmem_memcpy_persist'] = memmove_nodrain_all

        memset_nodrain_all = ['memset_nodrain_libc', 'memset_nodrain_generic', 'pmem_memset_nodrain.static', 'pmem_memset_nodrain_eadr.static']
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

        calls['pmem_memmove_nodrain.static'] = \
                            memmove_funcs['t']['noflush'] + \
                            memmove_funcs['nt']['flush'] + \
                            memmove_funcs['t']['flush']

        calls['pmem_memmove_nodrain_eadr.static'] = \
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

        calls['pmem_memset_nodrain.static'] = \
                            memsetfuncs['t']['noflush'] + \
                            memsetfuncs['nt']['flush'] + \
                            memsetfuncs['t']['flush']

        calls['pmem_memset_nodrain_eadr.static'] = \
                            memsetfuncs['t']['noflush'] + \
                            memsetfuncs['nt']['empty'] + \
                            memsetfuncs['t']['empty']

        return calls

def main():
        with open("white_list.json", 'r') as file:
                white_list = json.load(file)
        extra_calls = inlines({})
        extra_calls = function_pointers(extra_calls)
        config = {
                'filter': 'libpmem',
                'api': API,
                'dead_end': [],
                'extra_calls': extra_calls,
                'white_list': white_list
        }
        with open("config.json", "w") as outfile:
                json.dump(config, outfile, indent = 4)

if __name__ == '__main__':
        main()
