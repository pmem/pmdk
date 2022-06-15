sources = files(
    'obj_tx_add_range.c',
)

dependencies = [
    libpmemobj_dep,
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
