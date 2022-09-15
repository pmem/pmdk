libskiplist_map = library(
    'skiplist_map',
    'skiplist_map.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libskiplist_map_dep = declare_dependency(
    link_with: libskiplist_map,
    include_directories: include_directories('.'),
)
