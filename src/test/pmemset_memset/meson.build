sources = files(
    'pmemset_memset.c',
    '../pmem2_memset/memset_common.c',
)

dependencies = [
    libpmemset_dep,
    unittest_pmemset_dep,
]

c_args = []
include_directories = [
    include_directories('../pmem2_memset'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies + test_dependencies,
)
