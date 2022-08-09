sources = files(
    'obj_tx_add_range_direct.c',
)

dependencies = [
    libpmemobj_dep,
    unittest_dep,
    pmemcommon_dep,
]

c_args = []
include_directories = [
    include_directories('../../libpmemobj'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
