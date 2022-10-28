# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

manpages_7_md = files()
manpages_5_md = files()
manpages_3_md = files()
manpages_1_md = files()
manpages_3_dummy = files()

subdir('libpmem')
subdir('libpmem2')
subdir('libpmemblk')
subdir('libpmemlog')
subdir('libpmemobj')
subdir('libpmempool')
subdir('libpmemset')
subdir('pmem_ctl')
subdir('pmempool')
subdir('pmreorder')
subdir('poolset')

if get_option('rpmem')
    subdir('librpmem')
    subdir('rpmemd')
endif

if get_option('ndctl').enabled()
    subdir('daxio')
endif

manpages_md = manpages_7_md + manpages_5_md + manpages_3_md + manpages_1_md

if get_option('web')
    subdir('generated')
endif

message('Generating manpages...')
foreach file : manpages_md
    md2man = '../utils/md2man.sh'
    name = fs.stem(fs.name(file)) # for ex. libpmem.7
    dir = fs.name(fs.parent(file)) # for ex. libpmem
    dest = meson.current_build_dir()/dir/name # for ex. build/doc/libpmem/libpmem.7

    run_command(md2man, file, 'default.man', dest, check:true)
    message(''.join('Generated: ',  dest))

    if get_option('web')
        dest_windows = meson.current_build_dir()/'web_windows'/dir/name
        dest_linux = meson.current_build_dir()/'web_linux'/dir/name
        # It is not platform independent but docs can only be built on linux
        run_command('mkdir', '-p', meson.current_build_dir()/'web_windows'/dir, check:true)
        run_command('mkdir', '-p', meson.current_build_dir()/'web_linux'/dir, check:true)

        run_command(md2man, file, 'default.man', dest_windows, check:true, env:{'WEB':'1','WIN32':'1'})
        message(''.join('Generated: ',  dest_windows))
        run_command(md2man, file, 'default.man', dest_linux, check:true, env:{'WEB':'1','WIN32':''})
        message(''.join('Generated: ',  dest_linux))
    endif

    install_man(dest)
endforeach

foreach file : manpages_3_dummy
    install_man(file)
endforeach