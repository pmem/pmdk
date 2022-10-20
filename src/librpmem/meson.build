# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    '../core/alloc.c',
    '../core/os_posix.c',
    '../core/os_thread_posix.c',
    '../core/out.c',
    '../core/util.c',
    '../core/util_posix.c',
    'librpmem.c',
    'rpmem.c',
    'rpmem_obc.c',
    'rpmem_cmd.c',
    'rpmem_ssh.c',
    '../rpmem_common/rpmem_common.c',
    'rpmem_util.c',
    '../rpmem_common/rpmem_fip_common.c',
    'rpmem_fip.c'
)

librpmem = library(
    'rpmem',
    sources,
    c_args: [
        '-DRPMEMC_LOG_RPMEM',
    ],
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'librpmem.link.in'),
    ],
    dependencies: [
        libfabric_dep,
        threads_dep,
    ],
    include_directories: [
        include_directories('../rpmem_common'),
        common_include,
        core_include,
        root_include,
    ],
    soversion: '1',
    version: '1.0.0',
    install: true
)

librpmem_dep = declare_dependency(
    link_with: librpmem,
    include_directories: root_include,
)

librpmem_internal_dep = declare_dependency(
    sources: sources,
    compile_args: [
        '-DRPMEMC_LOG_RPMEM',
    ],
    dependencies: [
        libfabric_dep,
        threads_dep,
    ],
    include_directories: [
        include_directories('.'),
        include_directories('../rpmem_common'),
        common_include,
        core_include,
        root_include,
    ],
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('librpmem', librpmem_dep)
endif
