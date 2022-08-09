sources = files(
    'util_badblock.c',
)

dependencies = [
    libpmem_dep,
    pmemcommon_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmem2'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
