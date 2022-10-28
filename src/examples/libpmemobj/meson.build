# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include_directories = [
    include_directories('../'),
    include_directories('.'),
]

executable(
    'btree',
    'btree.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
)

executable(
    'buffons_needle_problem',
    'buffons_needle_problem.c',
    dependencies: [libpmemobj_dep, libm_dep],
    include_directories: include_directories,
)

executable(
    'lists',
    'lists.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
)

executable(
    'manpage',
    'manpage.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
)

executable(
    'pi',
    'pi.c',
    dependencies: [libpmemobj_dep, libm_dep, threads_dep],
    include_directories: include_directories,
)

executable(
    'setjmp',
    'setjmp.c',
    c_args: '-O2',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories,
)

subdir('hashmap')
subdir('libart')
subdir('linkedlist')
subdir('list_map')
subdir('pmemblk')
subdir('pmemlog')
subdir('queue')
subdir('slab_allocator')
subdir('string_store')
subdir('string_store_tx')
subdir('string_store_tx_type')
subdir('tree_map')
subdir('pminvaders')
subdir('map')
subdir('pmemobjfs')
