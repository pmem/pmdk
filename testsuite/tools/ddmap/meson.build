sources = files(
    'ddmap.c',
)

dependencies = [
    libpmem_dep,
    tools_common_dep,
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
