sources = files(
    'rpmem_obc_int.c',
)

dependencies = [
    libpmem_dep,
    librpmem_dep,
    pmemcommon_dep,
    rpmemd_dep,
    unittest_dep,
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
