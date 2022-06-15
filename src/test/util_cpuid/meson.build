sources = files(
    'util_cpuid.c',
)

dependencies = [
    libpmem2_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    '../../libpmem2/x86_64',
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
