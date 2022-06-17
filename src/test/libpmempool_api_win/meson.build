sources = files(
    'libpmempool_test_win.c',
)

dependencies = [
    unittest_dep,
	libpmempool_dep,
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
