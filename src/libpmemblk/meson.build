sources = files(
    'blk.c',
    'btt.c',
    'libpmemblk.c'
)

libpmemblk = library(
    'pmemblk',
    sources,
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemblk.link.in')
    ],
    dependencies: [
        libdaxctl_dep,
        libndctl_dep,
        libpmem_dep,
        pmemcommon_dep,
        pmemcore_dep,
        threads_dep,
    ],
    include_directories: root_include,
    soversion: '1',
    version: '1.0.0',
    install: true
)

libpmemblk_dep = declare_dependency(
    link_with: libpmemblk,
    include_directories: root_include,
)

libpmemblk_internal_dep = declare_dependency(
    sources: sources,
    dependencies: [
        libdaxctl_dep,
        libndctl_dep,
        libpmem_dep,
        pmemcommon_dep,
        pmemcore_dep,
        threads_dep,
    ],
    include_directories: [
        include_directories('.'),
        root_include,
    ],
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('libpmemblk', libpmemblk_dep)
endif
