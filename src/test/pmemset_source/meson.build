sources = files(
    'pmemset_source.c',
)

dependencies = [
    libpmem2_dep,
    libpmemset_dep,
    unittest_pmemset_dep,
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
