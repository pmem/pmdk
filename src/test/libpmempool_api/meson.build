sources = files(
    'libpmempool_test.c',
)

dependencies = [
    libpmempool_dep,
    unittest_dep,
]

c_args = []
include_directories = []

executable(
    'libpmempool_test',
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)

# TODO: 'pmemspoil' and 'bttcreate' dependency
