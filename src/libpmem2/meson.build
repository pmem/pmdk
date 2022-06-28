sources = files()
compile_args = []
includes = [include_directories('.')]

if host_machine.cpu_family() == 'aarch64'
    sources += files('aarch64/init.c')
elif host_machine.cpu_family() == 'ppc64'
    sources += files('ppc64/init.c')
elif host_machine.cpu_family() == 'riscv64'
    sources += files('riscv64/init.c')
elif host_machine.cpu_family() == 'x86_64'
    sources += files(
        'x86_64/cpu.c',
        'x86_64/init.c',
        'x86_64/memcpy/memcpy_nt_avx.c',
        'x86_64/memcpy/memcpy_nt_sse2.c',
        'x86_64/memset/memset_nt_avx.c',
        'x86_64/memset/memset_nt_sse2.c',
        'x86_64/memcpy/memcpy_t_avx.c',
        'x86_64/memcpy/memcpy_t_sse2.c',
        'x86_64/memset/memset_t_avx.c',
        'x86_64/memset/memset_t_sse2.c',
    )

    if cc.compiles(
        '''
        #include <immintrin.h>
        #include <stdint.h>

        int main(void) {
            uint64_t v[8];
            __m512i zmm0 = _mm512_loadu_si512((__m512i *)&v);

            return 0;
        }
        ''',
        args: ['-mavx512f'],
        name: 'avx512f support'
    )
        sources += files(
            'x86_64/memcpy/memcpy_nt_avx512f.c',
            'x86_64/memcpy/memcpy_t_avx512f.c',
            'x86_64/memset/memset_nt_avx512f.c',
            'x86_64/memset/memset_t_avx512f.c',
        )

        compile_args += [
            '-DAVX512F_AVAILABLE=1',
            '-mavx512f',
        ]
    else
        compile_args += '-DAVX512F_AVAILABLE=0'
    endif

    if cc.compiles(
        '''
        #include <immintrin.h>
        #include <stdint.h>

        int main(){
            uint64_t v[8], w[8];
            _movdir64b(&v, &w);
            return 0;
        }
        ''',
        args: ['-mmovdir64b'],
        name: 'movdir64b support'
    )
        message('Compiled movdir64b')
        sources += files(
            'x86_64/memcpy/memcpy_nt_movdir64b.c',
            'x86_64/memset/memset_nt_movdir64b.c',
        )

        compile_args += [
            '-DMOVDIR64B_AVAILABLE=1',
            '-mmovdir64b',
        ]
    else
        compile_args += '-DMOVDIR64B_AVAILABLE=0'
    endif

    includes += include_directories('x86_64/memcpy', 'x86_64/memset')

    compile_args += '-mavx'
else
    error('pmdk does not support @0@'.format(host_machine.cpu_family()))
endif

includes += include_directories(host_machine.cpu_family())

pmem2arch_dep = declare_dependency(
    compile_args: compile_args,
    sources: sources,
    include_directories: includes
)

sources = files(
    'libpmem2.c',
    'config.c',
    'deep_flush.c',
    'errormsg.c',
    'map.c',
    'map_posix.c',
    'mcsafe_ops_posix.c',
    'memops_generic.c',
    'mover.c',
    'persist.c',
    'persist_posix.c',
    'pmem2_utils.c',
    'source.c',
    'source_posix.c',
    'vm_reservation.c',
    'vm_reservation_posix.c'
)

if host_machine.system() == 'linux'
    sources += files(
        'auto_flush_linux.c',
        'deep_flush_linux.c',
        'extent_linux.c',
        'pmem2_utils_linux.c'
    )

    if libndctl_dep.found()
        sources += files('pmem2_utils_ndctl.c')
    else
        sources += files('pmem2_utils_none.c')
    endif
else
    sources += files(
        'auto_flush_none.c',
        'deep_flush_none.c',
        'extent_none.c',
        'pmem2_utils_other.c'
    )
endif

if libndctl_dep.found()
    sources += files(
        'badblocks_ndctl.c',
        'region_namespace_ndctl.c',
        'numa_ndctl.c',
        'usc_ndctl.c'
    )
else
    sources += files(
        'badblocks_none.c',
        'region_namespace_none.c',
        'numa_none.c',
        'usc_none.c'
    )
endif

libpmem2 = library(
    'pmem2',
    sources,
    c_args: [
        '-DPMEM2_USE_MINIASYNC',
    ],
    link_args: [
        '-Wl,--version-script=@0@'.format(meson.current_source_dir() / 'libpmem2.link.in'),
    ],
    dependencies: [
        threads_dep,
        libndctl_dep,
        libdaxctl_dep,
        # pmemcommon_dep,
        pmemcore_dep,
        pmem2arch_dep,
    ],
    include_directories: [
        root_include,
        common_include,
        miniasync_include,
    ],
    soversion: '1',
    version: '1.0.0',
    install: true
)

libpmem2_dep = declare_dependency(
    link_with: libpmem2,
    include_directories: [
        include_directories('.'),
    ],
)

if meson.version().version_compare('>=0.54')
    meson.override_dependency('libpmem2', libpmem2_dep)
endif
