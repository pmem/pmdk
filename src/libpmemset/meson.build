# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'badblock.c',
    'config.c',
    'errormsg.c',
    'file.c',
    'file_posix.c',
    'libpmemset.c',
    'map_config.c',
    'part.c',
    'pmemset.c',
    'pmemset_utils.c',
    'sds.c',
    'source.c'
)

libpmemset = library(
    'pmemset',
    sources,
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemset.link.in'),
    ],
    dependencies: [
        libpmem2_dep,
        pmemcore_dep,
        threads_dep,
    ],
    include_directories: [
        common_include,
        root_include,
    ],
    version: '1.0.0',
    soversion: '1',
    install: true,
)

libpmemset_dep = declare_dependency(
    link_with: libpmemset,
    include_directories: root_include,
)

libpmemset_internal_dep = declare_dependency(
    sources: sources,
    dependencies: [
        libpmem2_dep,
        pmemcore_dep,
        threads_dep,
    ],
    include_directories: [
        include_directories('.'),
        common_include,
        root_include,
    ],
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('libpmemset', libpmemset_dep)
endif
