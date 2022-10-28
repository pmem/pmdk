# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'cpufd.c',
    '../../../src/libpmem2/x86_64/cpu.c',
)

dependencies = [
    pmemcore_dep,
]

c_args = []
include_directories = [
    core_include,
    include_directories('../../../src/libpmem2/x86_64'),
]

executable(
    ttool,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
