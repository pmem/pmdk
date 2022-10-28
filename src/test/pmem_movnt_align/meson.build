# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'pmem_movnt_align.c',
    '../pmem2_movnt_align/movnt_align_common.c',
)

dependencies = [
    libpmem_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../pmem2_movnt_align'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)

copy_target = custom_target(t+'(test-files)',
    input : test_files,
    output :  test_files,
    command : [copy, '-i', '@INPUT@', '-o', '@OUTPUT@'],
    install : false,
    build_by_default : true
)
