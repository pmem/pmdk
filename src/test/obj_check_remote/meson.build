sources = files(
    'obj_check_remote.c',
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

# TODO: 'scp_to_remote_nodes' dependency
