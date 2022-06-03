sources = files(
    'ctl_cow.c',
)

dependencies = [
    libpmemblk_dep,
    libpmemlog_dep,
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
