tools = {
    'daxio': {
        'sources': files('daxio/daxio.c'),
        'include_directories': [
            core_include,
            common_include,
        ],
        'dependencies': [
            libpmem_dep,
            libndctl_dep,
            libdaxctl_dep,
        ],
    },
    'pmempool': {
        'c_args': ['-DUSE_RPMEM'],
        'sources': files(
            'pmempool/check.c',
            'pmempool/common.c',
            'pmempool/convert.c',
            'pmempool/create.c',
            'pmempool/dump.c',
            'pmempool/feature.c',
            'pmempool/info_blk.c',
            'pmempool/info_log.c',
            'pmempool/info_obj.c',
            'pmempool/info.c',
            'pmempool/output.c',
            'pmempool/pmempool.c',
            'pmempool/rm.c',
            'pmempool/synchronize.c',
            'pmempool/transform.c',
            '../librpmem/rpmem_cmd.c',
            '../librpmem/rpmem_ssh.c',
            '../librpmem/rpmem_util.c',
            '../rpmem_common/rpmem_common.c',
        ),
        'objects': [
            libpmemblk.extract_objects('btt.c'),
            libpmemobj.extract_objects('alloc_class.c', 'memblock.c', 'memops.c', 'ulog.c'),
        ],
        'include_directories': [
            root_include,
            include_directories('../librpmem'),
            include_directories('../rpmem_common'),
        ],
        'dependencies': [
            pmemcore_dep,
            pmemcommon_dep,
            libndctl_dep,
            libdaxctl_dep,
            libpmem_dep,
            libpmemblk_dep,
            libpmemlog_dep,
            libpmemobj_dep,
            libpmempool_dep,
        ],
    },
    'rpmemd': {
        'enabled': get_option('rpmem'),
        'c_args': ['-DRPMEMC_LOG_RPMEMD'],
        'sources': files(
            'rpmemd/rpmemd.c',
            'rpmemd/rpmemd_log.c',
            'rpmemd/rpmemd_config.c',
            '../rpmem_common/rpmem_common.c',
            'rpmemd/rpmemd_obc.c',
            'rpmemd/rpmemd_db.c',
            'rpmemd/rpmemd_fip.c',
            '../rpmem_common/rpmem_fip_common.c',
            'rpmemd/rpmemd_util.c'
        ),
        'dependencies': [
            threads_dep,
            libfabric_dep,
            libpmem_dep,
        ],
        'include_directories': [
            include_directories('rpmemd'),
            core_include,
            common_include,
            root_include,
            include_directories('../rpmem_common'),
        ],
    },
}

foreach t, params : tools
    if not params.get('enabled', true)
        continue
    endif

    executable(
        t,
        params.get('sources'),
        objects: params.get('objects', []),
        c_args: params.get('c_args', []),
        dependencies: params.get('dependencies', []),
        include_directories: params.get('include_directories', []),
        install: true
    )
endforeach
