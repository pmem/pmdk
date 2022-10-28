# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'extents.c',
)

dependencies = [
    pmemcore_dep,
    libndctl_dep,
    libdaxctl_dep,
]

c_args = []
include_directories = [root_include, '../../../src/libpmem2']
objects = [libpmem2.extract_objects('pmem2_utils_linux.c', 'pmem2_utils.c')]

if libndctl_dep.found()
    objects += libpmem2.extract_objects('region_namespace_ndctl.c')
else
    objects += libpmem2.extract_objects('region_namespace_none.c')
endif

if host_machine.system() == 'linux'
    objects += libpmem2.extract_objects('extent_linux.c')
else
    objects += libpmem2.extract_objects('extent_none.c')
endif

executable(
    ttool,
    sources,
    objects: objects,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
