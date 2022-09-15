libhashmap_atomic = library(
    'hashmap_atomic',
    'hashmap_atomic.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libhashmap_atomic_dep = declare_dependency(
    link_with: libhashmap_atomic,
    include_directories: include_directories('.'),
)

libhashmap_tx = library(
    'hashmap_tx',
    'hashmap_tx.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libhashmap_tx_dep = declare_dependency(
    link_with: libhashmap_tx,
    include_directories: include_directories('.'),
)

libhashmap_rp = library(
    'hashmap_rp',
    'hashmap_rp.c',
    dependencies: [libpmemobj_dep],
    include_directories: include_directories('.'),
    install: false,
)

libhashmap_rp_dep = declare_dependency(
    link_with: libhashmap_rp,
    include_directories: include_directories('.'),
)
