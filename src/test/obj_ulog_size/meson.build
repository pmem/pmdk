sources = files(
    'obj_ulog_size.c',
)

dependencies = [
    libpmemobj_dep,
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
