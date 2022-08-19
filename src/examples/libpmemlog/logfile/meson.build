include_directories = [
    include_directories('../../'),
    include_directories('.'),
]

executable(
    'addlog',
    'addlog.c',
    dependencies: [libpmemlog_dep],
    include_directories: include_directories
)

executable(
    'printlog',
    'printlog.c',
    dependencies: [libpmemlog_dep],
    include_directories: include_directories
)
