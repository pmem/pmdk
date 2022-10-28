# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'pmemset_include.c',
)

dependencies = [
    libpmemset_dep,
    unittest_pmemset_dep,
    unittest_dep,
    libpmem2_dep,
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
