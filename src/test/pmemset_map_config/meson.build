sources = files(
    'pmemset_map_config.c',
)

dependencies = [
    libpmem2_dep,
    libpmemset_internal_dep,
    unittest_pmemset_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmemset'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
