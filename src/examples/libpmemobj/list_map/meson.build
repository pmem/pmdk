# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

libskiplist_map = static_library(
    'skiplist_map',
    'skiplist_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libskiplist_map_dep = declare_dependency(
    link_with: libskiplist_map,
    include_directories: include_directories('.'),
)
