sources = files(
    'pmem_is_pmem.c',
)

dependencies = [
    libpmem_dep,
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
