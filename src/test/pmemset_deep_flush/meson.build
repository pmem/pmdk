sources = files(
    'pmemset_deep_flush.c',
)

dependencies = [
    libpmempool_dep,
    unittest_dep,
    libpmemset_internal_dep,
    libpmem2_internal_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmem2'),
    include_directories('../../libpmemset'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
