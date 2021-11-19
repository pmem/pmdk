sources = files(
    'bad_blocks.c',
    'set_badblocks.c',
    'ctl.c',
    'ctl_prefault.c',
    'ctl_sds.c',
    'ctl_fallocate.c',
    'ctl_cow.c',
    'file.c',
    'file_posix.c',
    'mmap.c',
    'mmap_posix.c',
    'os_deep_linux.c',
    'pool_hdr.c',
    'rand.c',
    'set.c',
    'shutdown_state.c',
    'uuid.c',
    'uuid_@0@.c'.format(host_machine.system()),
    '../libpmem2/pmem2_utils.c',
    '../libpmem2/config.c',
    '../libpmem2/persist_posix.c',
    '../libpmem2/badblocks.c',
    '../libpmem2/source.c',
    '../libpmem2/source_posix.c'
)

if host_machine.system() == 'linux'
    sources += files(
        '../libpmem2/auto_flush_linux.c',
        '../libpmem2/deep_flush_linux.c',
        '../libpmem2/extent_linux.c',
        '../libpmem2/pmem2_utils_linux.c'
    )

    if libndctl_dep.found()
        sources += files(
            '../libpmem2/pmem2_utils_ndctl.c'
        )
    else
        sources += files(
            '../libpmem2/pmem2_utils_none.c'
        )
    endif
else
    sources += files(
        '../libpmem2/auto_flush_none.c',
        '../libpmem2/deep_flush_other.c',
        '../libpmem2/extent_other.c',
        '../libpmem2/pmem2_utils_other.c',
    )
endif

if libndctl_dep.found()
    sources += files(
        '../libpmem2/badblocks_ndctl.c',
        '../libpmem2/usc_ndctl.c',
        '../libpmem2/region_namespace_ndctl.c',
        '../libpmem2/numa_ndctl.c'
    )
else
    sources += files(
        '../libpmem2/badblocks_none.c',
        '../libpmem2/usc_none.c',
        '../libpmem2/region_namespace_none.c',
        '../libpmem2/numa_none.c'
    )
endif

pmemcommon_dep = declare_dependency(
    sources: sources,
    include_directories: [
        include_directories('.'),
        include_directories('../libpmem2'),
    ]
)
