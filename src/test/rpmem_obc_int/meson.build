# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'rpmem_obc_int.c',
    '../../librpmem/rpmem_cmd.c',
    '../../librpmem/rpmem_obc.c',
    '../../librpmem/rpmem_ssh.c',
    '../../librpmem/rpmem_util.c',
    '../../rpmem_common/rpmem_common.c',
    '../../rpmem_common/rpmem_fip_common.c',
)

dependencies = [
    libpmem_dep,
    pmemcommon_dep,
    rpmemd_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../librpmem'),
    include_directories('../../rpmem_common'),
    include_directories('../../tools/rpmemd'),
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
