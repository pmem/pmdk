sources = files(
    'pmemdetect.c',
)

dependencies = [
    libdaxctl_dep,
    libndctl_dep,
    libpmem_dep,
    libpmem2_dep,
    pmemcommon_dep,
    pmemcore_dep,
    threads_dep,
    unittest_dep,
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
