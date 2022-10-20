# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'mapexec.c'
)

dependencies = [
    pmemcore_dep,
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
