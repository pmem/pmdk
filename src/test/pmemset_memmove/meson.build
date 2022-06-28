sources = files(
    'pmemset_memmove.c',
    '../pmem2_memmove/memmove_common.c',
)

dependencies = [
    libpmem2_dep,
    libpmemset_internal_dep,
    unittest_pmemset_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmem2'),
    include_directories('../pmem2_memmove'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
