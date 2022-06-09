sources = files(
    'obj_ctl_alloc_class_config.c',
)

dependencies = [
    libpmemobj_dep,
]

c_args = []
include_directories = []

executable(
    t,
    sources,
    c_args: c_args,
    include_directories: include_directories,
    dependencies: dependencies + test_dependencies,
)
