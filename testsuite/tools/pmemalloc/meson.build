sources = files(
    'pmemalloc.c'
)

dependencies = [
    libpmem_dep,
    pmemcommon_dep,
    libpmemobj_internal_dep,
]

c_args = []
include_directories = []

executable(
    ttool,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
