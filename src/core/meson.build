sources = files(
    'alloc.c',
    'fs_posix.c',
    'membuf.c',
    'os_posix.c',
    'os_thread_posix.c',
    'out.c',
    'ravl.c',
    'ravl_interval.c',
    'util.c',
    'util_posix.c'
)

pmemcore_dep = declare_dependency(
    sources: sources,
    include_directories: include_directories('.')
)
