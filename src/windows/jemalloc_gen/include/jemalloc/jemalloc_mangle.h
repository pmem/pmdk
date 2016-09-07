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
#  define pool_create je_pool_create
#  define pool_delete je_pool_delete
#  define pool_malloc je_pool_malloc
#  define pool_calloc je_pool_calloc
#  define pool_ralloc je_pool_ralloc
#  define pool_aligned_alloc je_pool_aligned_alloc
#  define pool_free je_pool_free
#  define pool_malloc_usable_size je_pool_malloc_usable_size
#  define pool_malloc_stats_print je_pool_malloc_stats_print
#  define pool_extend je_pool_extend
#  define pool_set_alloc_funcs je_pool_set_alloc_funcs
#  define pool_check je_pool_check
#  define malloc_conf je_malloc_conf
#  define malloc_message je_malloc_message
#  define malloc je_malloc
#  define calloc je_calloc
#  define posix_memalign je_posix_memalign
#  define aligned_alloc je_aligned_alloc
#  define realloc je_realloc
#  define free je_free
#  define mallocx je_mallocx
#  define rallocx je_rallocx
#  define xallocx je_xallocx
#  define sallocx je_sallocx
#  define dallocx je_dallocx
#  define nallocx je_nallocx
#  define mallctl je_mallctl
#  define mallctlnametomib je_mallctlnametomib
#  define mallctlbymib je_mallctlbymib
#  define navsnprintf je_navsnprintf
#  define malloc_stats_print je_malloc_stats_print
#  define malloc_usable_size je_malloc_usable_size
#endif

/*
 * The je_* macros can be used as stable alternative names for the
 * public jemalloc API if JEMALLOC_NO_DEMANGLE is defined.  This is primarily
 * meant for use in jemalloc itself, but it can be used by application code to
 * provide isolation from the name mangling specified via --with-mangling
 * and/or --with-jemalloc-prefix.
 */
#ifndef JEMALLOC_NO_DEMANGLE
#  undef je_pool_create
#  undef je_pool_delete
#  undef je_pool_malloc
#  undef je_pool_calloc
#  undef je_pool_ralloc
#  undef je_pool_aligned_alloc
#  undef je_pool_free
#  undef je_pool_malloc_usable_size
#  undef je_pool_malloc_stats_print
#  undef je_pool_extend
#  undef je_pool_set_alloc_funcs
#  undef je_pool_check
#  undef je_malloc_conf
#  undef je_malloc_message
#  undef je_malloc
#  undef je_calloc
#  undef je_posix_memalign
#  undef je_aligned_alloc
#  undef je_realloc
#  undef je_free
#  undef je_mallocx
#  undef je_rallocx
#  undef je_xallocx
#  undef je_sallocx
#  undef je_dallocx
#  undef je_nallocx
#  undef je_mallctl
#  undef je_mallctlnametomib
#  undef je_mallctlbymib
#  undef je_navsnprintf
#  undef je_malloc_stats_print
#  undef je_malloc_usable_size
#endif
