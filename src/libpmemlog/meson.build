sources = files(
    'libpmemlog.c',
    'log.c'
)

libpmemlog = library(
    'pmemlog',
    sources,
    c_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemlog.link.in')
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

libpmemlog_dep = declare_dependency(
    link_with: libpmemlog,
    include_directories: include_directories('.')
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('libpmemlog', libpmemblk_dep)
endif
