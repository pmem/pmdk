sources = files(
    'pmem_memset.c',
    '../pmem2_memset/memset_common.c',
)

dependencies = [
    libpmem_dep,
    unittest_dep,
]

c_args = []
include_directories = [
	    include_directories('../pmem2_memset') # testcase needs memset_common.h
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
