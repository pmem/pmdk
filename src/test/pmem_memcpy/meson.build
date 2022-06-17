sources = files(
    'pmem_memcpy.c',
    '../pmem2_memcpy/memcpy_common.c',
)

dependencies = [
    libpmem_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../pmem2_memcpy') # testcase needs memcpy_common.h
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
