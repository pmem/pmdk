# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

sources = files(
    'rpmemd_obc_test_close.c',
    'rpmemd_obc_test_common.c',
    'rpmemd_obc_test_create.c',
    'rpmemd_obc_test_misc.c',
    'rpmemd_obc_test_msg_hdr.c',
    'rpmemd_obc_test_open.c',
    'rpmemd_obc_test_set_attr.c',
    'rpmemd_obc_test.c',
    '../../librpmem/rpmem_cmd.c',
    '../../librpmem/rpmem_fip.c',
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
    include_directories('../../tools/rpmemd'),
    include_directories('../../rpmem_common'),
    include_directories('../../librpmem'),
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
