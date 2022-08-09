sources = files(
    'rpmem_obc_test_close.c',
    'rpmem_obc_test_common.c',
    'rpmem_obc_test_create.c',
    'rpmem_obc_test_misc.c',
    'rpmem_obc_test_open.c',
    'rpmem_obc_test_set_attr.c',
    'rpmem_obc_test.c',
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
    unittest_dep,
    libfabric_dep,
]

c_args = []
include_directories = [
    include_directories('../../librpmem'),
    include_directories('../../rpmem_common'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
