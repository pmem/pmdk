sources = files(
    'badblock.c',
    'config.c',
    'errormsg.c',
    'file.c',
    'file_posix.c',
    'libpmemset.c',
    'map_config.c',
    'part.c',
    'pmemset.c',
    'pmemset_utils.c',
    'sds.c',
    'source.c'
)

libpmemset = library(
    'pmemset',
    sources,
    c_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemset.link.in'),
    ],
    include_directories: [
        root_include,
        common_include,
    ],
    dependencies: [
        threads_dep,
        pmemcore_dep,
        libpmem2_dep,
    ],
    version: '1.0.0',
    soversion: '1',
    install: true
)

libpmemset_dep = declare_dependency(
    link_with: libpmemset
)
