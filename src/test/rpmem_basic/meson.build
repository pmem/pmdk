sources = files(
    'rpmem_basic.c',
    '../../rpmem_common/rpmem_common.c',
    '../../rpmem_common/rpmem_fip_common.c',
)

dependencies = [
    libpmem_dep,
    librpmem_dep,
    pmemcommon_dep,
    unittest_dep,
    libfabric_dep,
]

c_args = []
include_directories = [
    include_directories('../../rpmem_common'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
