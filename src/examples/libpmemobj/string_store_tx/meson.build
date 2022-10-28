# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

executable(
    'writer',
    'writer.c',
    dependencies: [libpmemobj_dep, libpmem_dep, threads_dep],
    include_directories: include_directories('.'),
)

executable(
    'reader',
    'reader.c',
    dependencies: [libpmemobj_dep, libpmem_dep, threads_dep],
    include_directories: include_directories('.'),
)
