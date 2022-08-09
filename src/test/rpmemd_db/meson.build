sources = files(
    'rpmemd_db_test.c',
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
    include_directories('../../rpmem_common'),
    include_directories('../../librpmem'),
    include_directories('../../tools/rpmemd'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
