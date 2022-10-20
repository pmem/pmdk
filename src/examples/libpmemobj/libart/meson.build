# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

libart = library(
    'art',
    'art.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libart_dep = declare_dependency(
    link_with: libart,
    include_directories: include_directories('.'),
)

executable(
    'arttree',
    'arttree.c',
    dependencies: [libpmemobj_dep, libart_dep],
    include_directories: include_directories('.'),
)

executable(
    'examine_arttree',
    ['arttree_examine.c','arttree_search.c', 'arttree_structures.c'],
    dependencies: [libpmemobj_dep, libart_dep],
    include_directories: include_directories('.'),
)
