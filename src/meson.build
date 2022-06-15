root_include = include_directories('include')
common_include = include_directories('common')
core_include = include_directories('core')

subdir('common')
subdir('core')
subdir('libpmem2')
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

if get_option('tests')
    subdir('test')
endif
