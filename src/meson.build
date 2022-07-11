root_include = include_directories('include')
common_include = include_directories('common')
core_include = include_directories('core')

subdir('core')
subdir('libpmem2')
subdir('common')
subdir('libpmem')
subdir('libpmemblk')
subdir('libpmemlog')
subdir('libpmemobj')
subdir('libpmempool')
subdir('libpmemset')
subdir('examples')

if get_option('rpmem')
    subdir('librpmem')
endif

if get_option('tools')
    subdir('tools')
endif

if get_option('tests') or get_option('tests-case')!=''
    subdir('test')
endif
