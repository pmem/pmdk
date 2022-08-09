sources = files(
    'blk_non_zero.c',
)

dependencies = [
    libpmemblk_dep,
    unittest_dep,
]

c_args = []

include_directories = [
    include_directories('../../libpmemblk'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
