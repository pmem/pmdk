sources = files(
    'pmem2_persist.c',
)

dependencies = [
    unittest_pmem2_dep,
    libpmem2_dep,
    libndctl_dep,
    libdaxctl_dep,
    libpmem_dep,
    pmemcore_dep,
    threads_dep,
    unittest_dep,
]

c_args = []

include_directories = [
    common_include,
    miniasync_include,
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
