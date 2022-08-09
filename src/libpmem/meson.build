sources = files(
    'libpmem.c',
    '../libpmem2/memops_generic.c',
    'pmem.c',
    'pmem_posix.c',
)

libpmem = library(
    'pmem',
    sources,
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmem.link.in'),
    ],
    dependencies: [
        libndctl_dep,
        libdaxctl_dep,
        pmemcommon_dep,
        pmemcore_dep,
        pmem2arch_dep,
    ],
    include_directories: [
        root_include,
    ],
    soversion: '1',
    version: '1.0.0',
    install: true
)

libpmem_dep = declare_dependency(
    link_with: libpmem,
    include_directories: root_include,
)

libpmem_internal_dep = declare_dependency(
    sources: sources,
    dependencies: [
        libndctl_dep,
        libdaxctl_dep,
        pmemcommon_dep,
        pmemcore_dep,
        pmem2arch_dep,
    ],
    include_directories: [
        include_directories('.'),
        root_include,
    ]
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('libpmem', libpmem_dep)
endif
