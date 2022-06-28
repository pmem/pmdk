sources = files(
    'pmem_memmove.c',
    '../pmem2_memmove/memmove_common.c',
)

dependencies = [
    libpmem_dep,
    unittest_dep,
    pmemcommon_dep,
]

c_args = []
include_directories = [
    include_directories('../pmem2_memmove'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
