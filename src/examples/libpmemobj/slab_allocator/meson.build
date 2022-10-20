# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

libslab_allocator = library(
    'slab_allocator',
    'slab_allocator.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libslab_allocator_dep = declare_dependency(
    link_with: libslab_allocator,
    include_directories: include_directories('.'),
)

executable(
    'main',
    'main.c',
    dependencies: [libpmemobj_dep, libslab_allocator_dep],
    include_directories: include_directories,
)
