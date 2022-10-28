# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'write.c'
)

dependencies = [
    libpmem_dep,
    libpmemblk_dep,
    libpmemlog_dep,
    tools_common_dep,
]

c_args = []
include_directories = []

executable(
    ttool,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
