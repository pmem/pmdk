sources = files(
    'pmem2_compat.c',
)

dependencies = [
    libpmem_dep,
    libpmem2_dep,
    unittest_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
