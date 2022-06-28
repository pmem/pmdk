sources = files(
    'obj_layout.c',
)

dependencies = [
    libpmemobj_internal_dep,
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
