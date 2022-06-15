sources = files(
    'pmem_movnt_align.c',
    '../pmem2_movnt_align/movnt_align_common.c',
)

dependencies = [
    libpmem_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../pmem2_movnt_align') # testcase needs movnt_align_common.h
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
