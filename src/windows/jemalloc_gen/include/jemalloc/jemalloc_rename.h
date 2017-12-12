/*
 * Name mangling for public symbols is controlled by --with-mangling and
 * --with-jemalloc-prefix.  With default settings the je_ prefix is stripped by
 * these macro definitions.
 */
#ifndef JEMALLOC_NO_RENAME
#  define je_pool_create je_vmem_pool_create
#  define je_pool_delete je_vmem_pool_delete
#  define je_pool_malloc je_vmem_pool_malloc
#  define je_pool_calloc je_vmem_pool_calloc
#  define je_pool_ralloc je_vmem_pool_ralloc
#  define je_pool_aligned_alloc je_vmem_pool_aligned_alloc
#  define je_pool_free je_vmem_pool_free
#  define je_pool_malloc_usable_size je_vmem_pool_malloc_usable_size
#  define je_pool_malloc_stats_print je_vmem_pool_malloc_stats_print
#  define je_pool_extend je_vmem_pool_extend
#  define je_pool_set_alloc_funcs je_vmem_pool_set_alloc_funcs
#  define je_pool_check je_vmem_pool_check
#  define je_malloc_conf je_vmem_malloc_conf
#  define je_malloc_message je_vmem_malloc_message
#  define je_malloc je_vmem_malloc
#  define je_calloc je_vmem_calloc
#  define je_posix_memalign je_vmem_posix_memalign
#  define je_aligned_alloc je_vmem_aligned_alloc
#  define je_realloc je_vmem_realloc
#  define je_free je_vmem_free
#  define je_mallocx je_vmem_mallocx
#  define je_rallocx je_vmem_rallocx
#  define je_xallocx je_vmem_xallocx
#  define je_sallocx je_vmem_sallocx
#  define je_dallocx je_vmem_dallocx
#  define je_nallocx je_vmem_nallocx
#  define je_mallctl je_vmem_mallctl
#  define je_mallctlnametomib je_vmem_mallctlnametomib
#  define je_mallctlbymib je_vmem_mallctlbymib
#  define je_navsnprintf je_vmem_navsnprintf
#  define je_malloc_stats_print je_vmem_malloc_stats_print
#  define je_malloc_usable_size je_vmem_malloc_usable_size
#endif
