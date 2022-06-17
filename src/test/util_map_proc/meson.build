sources = files(
    'util_map_proc.c',
)

dependencies = [
    libpmem_dep,
    pmemcommon_dep,
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
