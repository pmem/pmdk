sources = files(
    'pmem2_badblock.c',
)

dependencies = [
    libpmem2_internal_dep,
    unittest_pmem2_dep,
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
