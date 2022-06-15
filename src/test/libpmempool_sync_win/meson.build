sources = files(
    'libpmempool_sync_win.c',
)

dependencies = [
    libpmempool_dep,
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

# TODO: 'pmemspoil' and 'pmemobjcli' dependency
