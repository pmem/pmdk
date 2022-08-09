sources = files(
    'alloc_class.c',
    'bucket.c',
    'container_ravl.c',
    'container_seglists.c',
    'critnib.c',
    'ctl_debug.c',
    'heap.c',
    'lane.c',
    'libpmemobj.c',
    'list.c',
    'memblock.c',
    'memops.c',
    'obj.c',
    'palloc.c',
    'pmalloc.c',
    'recycler.c',
    'sync.c',
    'tx.c',
    'stats.c',
    'ulog.c',
)

libpmemobj = library(
    'pmemobj',
    sources,
    c_args: [
        '-DUSE_LIBDL',
        '-D_PMEMOBJ_INTRNL',
    ],
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmemobj.link.in'),
    ],
    dependencies: [
        libdaxctl_dep,
        libndctl_dep,
	libdl_dep,
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

libpmemobj_dep = declare_dependency(
    link_with: libpmemobj,
    include_directories: [
	root_include,
    ],
)

libpmemobj_internal_dep = declare_dependency(
    sources: sources,
    compile_args: [
        '-DUSE_LIBDL',
        '-D_PMEMOBJ_INTRNL',
    ],
    dependencies: [
        libdaxctl_dep,
        libndctl_dep,
	libdl_dep,
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
    meson.override_dependency('libpmemobj', libpmemblk_dep)
endif
