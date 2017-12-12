#ifndef JEMALLOC_H_
#define	JEMALLOC_H_
#ifdef __cplusplus
extern "C" {
#endif

/* Defined if __attribute__((...)) syntax is supported. */
/* #undef JEMALLOC_HAVE_ATTR */

/* Defined if alloc_size attribute is supported. */
/* #undef JEMALLOC_HAVE_ATTR_ALLOC_SIZE */

/* Defined if format(gnu_printf, ...) attribute is supported. */
/* #undef JEMALLOC_HAVE_ATTR_FORMAT_GNU_PRINTF */

/* Defined if format(printf, ...) attribute is supported. */
/* #undef JEMALLOC_HAVE_ATTR_FORMAT_PRINTF */

/*
 * Define overrides for non-standard allocator-related functions if they are
 * present on the system.
 */
/* #undef JEMALLOC_OVERRIDE_MEMALIGN */
/* #undef JEMALLOC_OVERRIDE_VALLOC */

/*
 * At least Linux omits the "const" in:
 *
 *   size_t malloc_usable_size(const void *ptr);
 *
 * Match the operating system's prototype.
 */
#define	JEMALLOC_USABLE_SIZE_CONST const

/*
 * If defined, specify throw() for the public function prototypes when compiling
 * with C++.  The only justification for this is to match the prototypes that
 * glibc defines.
 */
/* #undef JEMALLOC_USE_CXX_THROW */

#ifdef _MSC_VER
#  ifdef _WIN64
#    define LG_SIZEOF_PTR_WIN 3
#  else
#    define LG_SIZEOF_PTR_WIN 2
#  endif
#endif

/* sizeof(void *) == 2^LG_SIZEOF_PTR. */
#define	LG_SIZEOF_PTR LG_SIZEOF_PTR_WIN

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

#include <limits.h>
#include <strings.h>
#include <stdbool.h>
#include <stdarg.h>

#define	JEMALLOC_VERSION ""
#define	JEMALLOC_VERSION_MAJOR
#define	JEMALLOC_VERSION_MINOR
#define	JEMALLOC_VERSION_BUGFIX
#define	JEMALLOC_VERSION_NREV
#define	JEMALLOC_VERSION_GID ""

#  define MALLOCX_LG_ALIGN(la)	(la)
#  if LG_SIZEOF_PTR == 2
#    define MALLOCX_ALIGN(a)	(ffs(a)-1)
#  else
#    define MALLOCX_ALIGN(a)						\
	 ((a < (size_t)INT_MAX) ? ffs(a)-1 : ffs(a>>32)+31)
#  endif
#  define MALLOCX_ZERO	((int)0x40)
/* Bias arena index bits so that 0 encodes "MALLOCX_ARENA() unspecified". */
#  define MALLOCX_ARENA(a)	((int)(((a)+1) << 8))

#ifdef JEMALLOC_HAVE_ATTR
#  define JEMALLOC_ATTR(s) __attribute__((s))
#  define JEMALLOC_EXPORT JEMALLOC_ATTR(visibility("default"))
#  define JEMALLOC_ALIGNED(s) JEMALLOC_ATTR(aligned(s))
#  define JEMALLOC_SECTION(s) JEMALLOC_ATTR(section(s))
#  define JEMALLOC_NOINLINE JEMALLOC_ATTR(noinline)
#elif _MSC_VER
#  define JEMALLOC_ATTR(s)
#  ifndef JEMALLOC_EXPORT
#    ifdef DLLEXPORT
#      define JEMALLOC_EXPORT __declspec(dllexport)
#    else
#      define JEMALLOC_EXPORT __declspec(dllimport)
#    endif
#  endif
#  define JEMALLOC_ALIGNED(s) __declspec(align(s))
#  define JEMALLOC_SECTION(s) __declspec(allocate(s))
#  define JEMALLOC_NOINLINE __declspec(noinline)
#else
#  define JEMALLOC_ATTR(s)
#  define JEMALLOC_EXPORT
#  define JEMALLOC_ALIGNED(s)
#  define JEMALLOC_SECTION(s)
#  define JEMALLOC_NOINLINE
#endif

/*
 * The je_ prefix on the following public symbol declarations is an artifact
 * of namespace management, and should be omitted in application code unless
 * JEMALLOC_NO_DEMANGLE is defined (see jemalloc_mangle.h).
 */
extern JEMALLOC_EXPORT const char	*je_malloc_conf;
extern JEMALLOC_EXPORT void		(*je_malloc_message)(void *cbopaque,
    const char *s);

typedef struct pool_s pool_t;

JEMALLOC_EXPORT pool_t	*je_pool_create(void *addr, size_t size, int zeroed, int empty);
JEMALLOC_EXPORT int	je_pool_delete(pool_t *pool);
JEMALLOC_EXPORT size_t	je_pool_extend(pool_t *pool, void *addr,
					    size_t size, int zeroed);
JEMALLOC_EXPORT void	*je_pool_malloc(pool_t *pool, size_t size);
JEMALLOC_EXPORT void	*je_pool_calloc(pool_t *pool, size_t nmemb, size_t size);
JEMALLOC_EXPORT void	*je_pool_ralloc(pool_t *pool, void *ptr, size_t size);
JEMALLOC_EXPORT void	*je_pool_aligned_alloc(pool_t *pool,  size_t alignment, size_t size);
JEMALLOC_EXPORT void	je_pool_free(pool_t *pool, void *ptr);
JEMALLOC_EXPORT size_t	je_pool_malloc_usable_size(pool_t *pool, void *ptr);
JEMALLOC_EXPORT void	je_pool_malloc_stats_print(pool_t *pool,
							void (*write_cb)(void *, const char *),
							void *cbopaque, const char *opts);
JEMALLOC_EXPORT void	je_pool_set_alloc_funcs(void *(*malloc_func)(size_t),
							void (*free_func)(void *));
JEMALLOC_EXPORT int	je_pool_check(pool_t *pool);

JEMALLOC_EXPORT void	*je_malloc(size_t size) JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*je_calloc(size_t num, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT int	je_posix_memalign(void **memptr, size_t alignment,
    size_t size) JEMALLOC_ATTR(nonnull(1));
JEMALLOC_EXPORT void	*je_aligned_alloc(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*je_realloc(void *ptr, size_t size);
JEMALLOC_EXPORT void	je_free(void *ptr);

JEMALLOC_EXPORT void	*je_mallocx(size_t size, int flags);
JEMALLOC_EXPORT void	*je_rallocx(void *ptr, size_t size, int flags);
JEMALLOC_EXPORT size_t	je_xallocx(void *ptr, size_t size, size_t extra,
    int flags);
JEMALLOC_EXPORT size_t	je_sallocx(const void *ptr, int flags);
JEMALLOC_EXPORT void	je_dallocx(void *ptr, int flags);
JEMALLOC_EXPORT size_t	je_nallocx(size_t size, int flags);

JEMALLOC_EXPORT int	je_mallctl(const char *name, void *oldp,
    size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT int	je_mallctlnametomib(const char *name, size_t *mibp,
    size_t *miblenp);
JEMALLOC_EXPORT int	je_mallctlbymib(const size_t *mib, size_t miblen,
    void *oldp, size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT void	je_malloc_stats_print(void (*write_cb)(void *,
    const char *), void *je_cbopaque, const char *opts);
JEMALLOC_EXPORT size_t	je_malloc_usable_size(
    JEMALLOC_USABLE_SIZE_CONST void *ptr);

JEMALLOC_EXPORT int    je_navsnprintf(char *str, size_t size,
    const char *format, va_list ap);

#ifdef JEMALLOC_OVERRIDE_MEMALIGN
JEMALLOC_EXPORT void *	je_memalign(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
#endif

#ifdef JEMALLOC_OVERRIDE_VALLOC
JEMALLOC_EXPORT void *	je_valloc(size_t size) JEMALLOC_ATTR(malloc);
#endif

typedef void *(chunk_alloc_t)(void *, size_t, size_t, bool *, unsigned, pool_t *);
typedef bool (chunk_dalloc_t)(void *, size_t, unsigned, pool_t *);

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

#ifdef __cplusplus
}
#endif
#endif /* JEMALLOC_H_ */
