sources = files(
    'pmem2_memmove.c',
    'memmove_common.c',
)

dependencies = [
    libpmem2_dep,
    unittest_pmem2_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies + test_dependencies,
)
