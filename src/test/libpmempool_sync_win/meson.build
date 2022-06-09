sources = files(
    'libpmempool_sync_win.c',
)

dependencies = [
    libpmempool_dep,
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

# TODO: 'pmemspoil' and 'pmemobjcli' dependency
