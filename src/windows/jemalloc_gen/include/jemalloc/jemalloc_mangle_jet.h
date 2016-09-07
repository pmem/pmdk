/*
 * By default application code must explicitly refer to mangled symbol names,
 * so that it is possible to use jemalloc in conjunction with another allocator
 * in the same application.  Define JEMALLOC_MANGLE in order to cause automatic
 * name mangling that matches the API prefixing that happened as a result of
 * --with-mangling and/or --with-jemalloc-prefix configuration settings.
 */
#ifdef JEMALLOC_MANGLE
#  ifndef JEMALLOC_NO_DEMANGLE
#    define JEMALLOC_NO_DEMANGLE
#  endif
#  define pool_create jet_pool_create
#  define pool_delete jet_pool_delete
#  define pool_malloc jet_pool_malloc
#  define pool_calloc jet_pool_calloc
#  define pool_ralloc jet_pool_ralloc
#  define pool_aligned_alloc jet_pool_aligned_alloc
#  define pool_free jet_pool_free
#  define pool_malloc_usable_size jet_pool_malloc_usable_size
#  define pool_malloc_stats_print jet_pool_malloc_stats_print
#  define pool_extend jet_pool_extend
#  define pool_set_alloc_funcs jet_pool_set_alloc_funcs
#  define pool_check jet_pool_check
#  define malloc_conf jet_malloc_conf
#  define malloc_message jet_malloc_message
#  define malloc jet_malloc
#  define calloc jet_calloc
#  define posix_memalign jet_posix_memalign
#  define aligned_alloc jet_aligned_alloc
#  define realloc jet_realloc
#  define free jet_free
#  define mallocx jet_mallocx
#  define rallocx jet_rallocx
#  define xallocx jet_xallocx
#  define sallocx jet_sallocx
#  define dallocx jet_dallocx
#  define nallocx jet_nallocx
#  define mallctl jet_mallctl
#  define mallctlnametomib jet_mallctlnametomib
#  define mallctlbymib jet_mallctlbymib
#  define navsnprintf jet_navsnprintf
#  define malloc_stats_print jet_malloc_stats_print
#  define malloc_usable_size jet_malloc_usable_size
#endif

/*
 * The jet_* macros can be used as stable alternative names for the
 * public jemalloc API if JEMALLOC_NO_DEMANGLE is defined.  This is primarily
 * meant for use in jemalloc itself, but it can be used by application code to
 * provide isolation from the name mangling specified via --with-mangling
 * and/or --with-jemalloc-prefix.
 */
#ifndef JEMALLOC_NO_DEMANGLE
#  undef jet_pool_create
#  undef jet_pool_delete
#  undef jet_pool_malloc
#  undef jet_pool_calloc
#  undef jet_pool_ralloc
#  undef jet_pool_aligned_alloc
#  undef jet_pool_free
#  undef jet_pool_malloc_usable_size
#  undef jet_pool_malloc_stats_print
#  undef jet_pool_extend
#  undef jet_pool_set_alloc_funcs
#  undef jet_pool_check
#  undef jet_malloc_conf
#  undef jet_malloc_message
#  undef jet_malloc
#  undef jet_calloc
#  undef jet_posix_memalign
#  undef jet_aligned_alloc
#  undef jet_realloc
#  undef jet_free
#  undef jet_mallocx
#  undef jet_rallocx
#  undef jet_xallocx
#  undef jet_sallocx
#  undef jet_dallocx
#  undef jet_nallocx
#  undef jet_mallctl
#  undef jet_mallctlnametomib
#  undef jet_mallctlbymib
#  undef jet_navsnprintf
#  undef jet_malloc_stats_print
#  undef jet_malloc_usable_size
#endif
