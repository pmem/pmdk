/*
 * The jet_ prefix on the following public symbol declarations is an artifact
 * of namespace management, and should be omitted in application code unless
 * JEMALLOC_NO_DEMANGLE is defined (see jemalloc_mangle@install_suffix@.h).
 */
extern JEMALLOC_EXPORT const char	*jet_malloc_conf;
extern JEMALLOC_EXPORT void		(*jet_malloc_message)(void *cbopaque,
    const char *s);

typedef struct pool_s pool_t;

JEMALLOC_EXPORT pool_t	*jet_pool_create(void *addr, size_t size, int zeroed);
JEMALLOC_EXPORT int	jet_pool_delete(pool_t *pool);
JEMALLOC_EXPORT size_t	jet_pool_extend(pool_t *pool, void *addr,
					    size_t size, int zeroed);
JEMALLOC_EXPORT void	*jet_pool_malloc(pool_t *pool, size_t size);
JEMALLOC_EXPORT void	*jet_pool_calloc(pool_t *pool, size_t nmemb, size_t size);
JEMALLOC_EXPORT void	*jet_pool_ralloc(pool_t *pool, void *ptr, size_t size);
JEMALLOC_EXPORT void	*jet_pool_aligned_alloc(pool_t *pool,  size_t alignment, size_t size);
JEMALLOC_EXPORT void	jet_pool_free(pool_t *pool, void *ptr);
JEMALLOC_EXPORT size_t	jet_pool_malloc_usable_size(pool_t *pool, void *ptr);
JEMALLOC_EXPORT void	jet_pool_malloc_stats_print(pool_t *pool,
							void (*write_cb)(void *, const char *),
							void *cbopaque, const char *opts);
JEMALLOC_EXPORT void	jet_pool_set_alloc_funcs(void *(*malloc_func)(size_t),
							void (*free_func)(void *));
JEMALLOC_EXPORT int	jet_pool_check(pool_t *pool);

JEMALLOC_EXPORT void	*jet_malloc(size_t size) JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*jet_calloc(size_t num, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT int	jet_posix_memalign(void **memptr, size_t alignment,
    size_t size) JEMALLOC_ATTR(nonnull(1));
JEMALLOC_EXPORT void	*jet_aligned_alloc(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
JEMALLOC_EXPORT void	*jet_realloc(void *ptr, size_t size);
JEMALLOC_EXPORT void	jet_free(void *ptr);

JEMALLOC_EXPORT void	*jet_mallocx(size_t size, int flags);
JEMALLOC_EXPORT void	*jet_rallocx(void *ptr, size_t size, int flags);
JEMALLOC_EXPORT size_t	jet_xallocx(void *ptr, size_t size, size_t extra,
    int flags);
JEMALLOC_EXPORT size_t	jet_sallocx(const void *ptr, int flags);
JEMALLOC_EXPORT void	jet_dallocx(void *ptr, int flags);
JEMALLOC_EXPORT size_t	jet_nallocx(size_t size, int flags);

JEMALLOC_EXPORT int	jet_mallctl(const char *name, void *oldp,
    size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT int	jet_mallctlnametomib(const char *name, size_t *mibp,
    size_t *miblenp);
JEMALLOC_EXPORT int	jet_mallctlbymib(const size_t *mib, size_t miblen,
    void *oldp, size_t *oldlenp, void *newp, size_t newlen);
JEMALLOC_EXPORT void	jet_malloc_stats_print(void (*write_cb)(void *,
    const char *), void *jet_cbopaque, const char *opts);
JEMALLOC_EXPORT size_t	jet_malloc_usable_size(
    JEMALLOC_USABLE_SIZE_CONST void *ptr);

JEMALLOC_EXPORT int    jet_navsnprintf(char *str, size_t size,
    const char *format, va_list ap);

#ifdef JEMALLOC_OVERRIDE_MEMALIGN
JEMALLOC_EXPORT void *	jet_memalign(size_t alignment, size_t size)
    JEMALLOC_ATTR(malloc);
#endif

#ifdef JEMALLOC_OVERRIDE_VALLOC
JEMALLOC_EXPORT void *	jet_valloc(size_t size) JEMALLOC_ATTR(malloc);
#endif
