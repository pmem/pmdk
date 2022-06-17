sources = files(
    'obj_direct.c',
    'obj_direct_inline.c',
    'obj_direct_non_inline.c',
)

dependencies = [
    libpmemobj_dep,
    unittest_dep,
]

c_args = []
include_directories = [
    include_directories('.'),
]

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies,
)
