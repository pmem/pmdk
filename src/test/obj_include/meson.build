# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'obj_atomic_base_include.c',
    'obj_atomic_include.c',
    'obj_base_include.c',
    'obj_iterator_base_include.c',
    'obj_iterator_include.c',
    'obj_lists_atomic_base_include.c',
    'obj_lists_atomic_include.c',
    'obj_pool_base_include.c',
    'obj_pool_include.c',
    'obj_thread_include.c',
    'obj_tx_base_include.c',
    'obj_tx_include.c',
    'obj_types_include.c',
)

dependencies = [
    libpmemobj_dep,
    unittest_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
