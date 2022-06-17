sources = files(
    'pmemset_deep_flush.c',
)

dependencies = [
    libpmempool_dep,
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
