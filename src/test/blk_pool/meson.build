sources = files(
    'blk_pool.c',
)

dependencies = [
    unittest_dep,
	libpmemblk_dep,
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

# TODO: 'pmemspoil' dependency