sources = files(
    'anonymous_mmap.c',
)

dependencies = [
    libpmem_dep,
    pmemcommon_dep,
    pmemcore_dep,
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
