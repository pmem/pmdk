sources = files(
    'rpmemd_dbg.c',
)

dependencies = [
    librpmem_dep,
    rpmemd_dep,
    unittest_dep,
    pmemcommon_dep,
]

c_args = []
include_directories = [
    include_directories('../../tools/rpmemd'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
