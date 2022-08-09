sources = files(
    'obj_tx_add_range.c',
)

dependencies = [
    libpmemobj_dep,
    pmemcore_dep,
#     pmemcommon_dep,
    unittest_dep,
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
