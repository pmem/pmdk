sources = files(
    'obj_pmalloc_mt.c',
)

dependencies = [
    libpmem_dep,
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
