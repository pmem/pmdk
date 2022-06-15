sources = files(
    'pmemset_memset.c',
    '../pmem2_memset/memset_common.c',
)

dependencies = [
    libpmem2_dep,
    libpmemset_dep,
    unittest_pmemset_dep,
    unittest_dep,
    unittest_dep,
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
    dependencies: dependencies,
)
