# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'pmem2_future.c',
)

dependencies = [
    libpmem2_dep,
    miniasync_dep,
    unittest_pmem2_dep,
    unittest_dep,
]

c_args = []
link_args = []
include_directories = [
    miniasync_include,
]

executable(
    t,
    sources,
    c_args: c_args,
    link_args: link_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
