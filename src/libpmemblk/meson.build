sources = files(
    'blk.c',
    'btt.c',
    'libpmemblk.c'
)

libpmemblk = library(
    'pmemblk',
    sources,
    c_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemblk.link.in')
    ],
    dependencies: [
        threads_dep,
        libdaxctl_dep,
        libndctl_dep,
        pmemcommon_dep,
        pmemcore_dep,
        libpmem_dep,
    ],
    include_directories: root_include,
    soversion: '1',
    version: '1.0.0',
    install: true
)

libpmemblk_dep = declare_dependency(
    link_with: libpmemblk,
    include_directories: include_directories('.')
)

meson.override_dependency('libpmemblk', libpmemblk_dep)
