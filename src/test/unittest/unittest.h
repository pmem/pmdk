/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * unittest.h -- the mundane stuff shared by all unit tests
 *
 * we want unit tests to be very thorough and check absolutely everything
 * in order to nail down the test case as precisely as possible and flag
 * anything at all unexpected.  as a result, most unit tests are 90% code
 * checking stuff that isn't really interesting to what is being tested.
 * to help address this, the macros defined here include all the boilerplate
 * error checking which prints information and exits on unexpected errors.
 *
 * the result changes this code:
 *
 *	if ((buf = malloc(size)) == NULL) {
 *		fprintf(stderr, "cannot allocate %d bytes for buf\n", size);
 *		exit(1);
 *	}
 *
 * into this code:
 *
 *	buf = MALLOC(size);
 *
 * and the error message includes the calling context information (file:line).
 * in general, using the all-caps version of a call means you're using the
 * unittest.h version which does the most common checking for you.  so
 * calling MALLOC() instead of malloc() returns the same
 * thing, but can never return an error since the unit test library checks for
 * it.
 *
 * all unit tests should use the same initialization:
 *
 *	START(argc, argv, "brief test description", ...);
 *
 * all unit tests should use these exit calls:
 *
 *	DONE("message", ...);
 *	UT_FATAL("message", ...);
 *
 * uniform stderr and stdout messages:
 *
 *	UT_OUT("message", ...);
 *	UT_ERR("message", ...);
 *
 * in all cases above, the message is printf-like, taking variable args.
 * the message can be NULL.  it can start with "!" in which case the "!" is
 * skipped and the message gets the errno string appended to it, like this:
 *
 *	if (somesyscall(..) < 0)
 *		UT_FATAL("!my message");
 */

#ifndef _UNITTEST_H
#define _UNITTEST_H 1

#include <libpmem.h>
#include <libpmem2.h>
#include <libpmemobj.h>
#include <libpmempool.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <setjmp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>

/* XXX: move OS abstraction layer out of common */
#include "os.h"
#include "os_thread.h"
#include "util.h"

int ut_get_uuid_str(char *);
#define UT_MAX_ERR_MSG 128
#define UT_POOL_HDR_UUID_STR_LEN 37 /* uuid string length */
#define UT_POOL_HDR_UUID_GEN_FILE "/proc/sys/kernel/random/uuid"

/*
 * unit test support...
 */
void ut_start(const char *file, int line, const char *func,
	int argc, char * const argv[], const char *fmt, ...)
	__attribute__((format(printf, 6, 7)));

void NORETURN ut_done(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void NORETURN ut_fatal(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void NORETURN ut_end(const char *file, int line, const char *func,
	int ret);
void ut_out(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void ut_err(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));

/* indicate the start of the test */
#define START(argc, argv, ...)\
    ut_start(__FILE__, __LINE__, __func__, argc, argv, __VA_ARGS__)

/* normal exit from test */
#define DONE(...)\
    ut_done(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define DONEW(...)\
    ut_done(__FILE__, __LINE__, __func__, __VA_ARGS__)

#define END(ret, ...)\
    ut_end(__FILE__, __LINE__, __func__, ret)

/* fatal error detected */
#define UT_FATAL(...)\
    ut_fatal(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* normal output */
#define UT_OUT(...)\
    ut_out(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* error output */
#define UT_ERR(...)\
    ut_err(__FILE__, __LINE__, __func__, __VA_ARGS__)

/*
 * assertions...
 */

/* assert a condition is true at runtime */
#define UT_ASSERT_rt(cnd)\
	((void)((cnd) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s", #cnd), 0)))

/* assertion with extra info printed if assertion fails at runtime */
#define UT_ASSERTinfo_rt(cnd, info) \
	((void)((cnd) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (%s)", #cnd, info), 0)))

/* assert two integer values are equal at runtime */
#define UT_ASSERTeq_rt(lhs, rhs)\
	((void)(((lhs) == (rhs)) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) == %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

/* assert two integer values are not equal at runtime */
#define UT_ASSERTne_rt(lhs, rhs)\
	((void)(((lhs) != (rhs)) || (ut_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) != %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

#if defined(__CHECKER__)
#define UT_COMPILE_ERROR_ON(cond)
#define UT_ASSERT_COMPILE_ERROR_ON(cond)
#else
#define UT_COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#ifndef __cplusplus
#define UT_ASSERT_COMPILE_ERROR_ON(cond) UT_COMPILE_ERROR_ON(cond)
#else /* __cplusplus */
/*
 * XXX - workaround for https://github.com/pmem/issues/issues/189
 */
#define UT_ASSERT_COMPILE_ERROR_ON(cond) UT_ASSERT_rt(!(cond))
#endif /* __cplusplus */
#endif /* __CHECKER__ */

/* assert a condition is true */
#define UT_ASSERT(cnd)\
	do {\
		/*\
		 * Detect useless asserts on always true expression. Please use\
		 * UT_COMPILE_ERROR_ON(!cnd) or UT_ASSERT_rt(cnd) in such\
		 * cases.\
		 */\
		if (__builtin_constant_p(cnd))\
			UT_ASSERT_COMPILE_ERROR_ON(cnd);\
		UT_ASSERT_rt(cnd);\
	} while (0)

/* assertion with extra info printed if assertion fails */
#define UT_ASSERTinfo(cnd, info) \
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(cnd))\
			UT_ASSERT_COMPILE_ERROR_ON(cnd);\
		UT_ASSERTinfo_rt(cnd, info);\
	} while (0)

/* assert two integer values are equal */
#define UT_ASSERTeq(lhs, rhs)\
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
			UT_ASSERT_COMPILE_ERROR_ON((lhs) == (rhs));\
		UT_ASSERTeq_rt(lhs, rhs);\
	} while (0)

/* assert two integer values are not equal */
#define UT_ASSERTne(lhs, rhs)\
	do {\
		/* See comment in UT_ASSERT. */\
		if (__builtin_constant_p(lhs) && __builtin_constant_p(rhs))\
			UT_ASSERT_COMPILE_ERROR_ON((lhs) != (rhs));\
		UT_ASSERTne_rt(lhs, rhs);\
	} while (0)

/* assert pointer is fits range of [start, start + size) */
#define UT_ASSERTrange(ptr, start, size)\
	((void)(((uintptr_t)(ptr) >= (uintptr_t)(start) &&\
	(uintptr_t)(ptr) < (uintptr_t)(start) + (uintptr_t)(size)) ||\
	(ut_fatal(__FILE__, __LINE__, __func__,\
	"assert failure: %s (%p) is outside range [%s (%p), %s (%p))", #ptr,\
	(void *)(ptr), #start, (void *)(start), #start"+"#size,\
	(void *)((uintptr_t)(start) + (uintptr_t)(size))), 0)))

/*
 * memory allocation...
 */
void *ut_malloc(const char *file, int line, const char *func, size_t size);
void *ut_calloc(const char *file, int line, const char *func,
    size_t nmemb, size_t size);
void ut_free(const char *file, int line, const char *func, void *ptr);
void ut_aligned_free(const char *file, int line, const char *func, void *ptr);
void *ut_realloc(const char *file, int line, const char *func,
    void *ptr, size_t size);
char *ut_strdup(const char *file, int line, const char *func,
    const char *str);
void *ut_pagealignmalloc(const char *file, int line, const char *func,
    size_t size);
void *ut_memalign(const char *file, int line, const char *func,
    size_t alignment, size_t size);
void *ut_mmap_anon_aligned(const char *file, int line, const char *func,
    size_t alignment, size_t size);
int ut_munmap_anon_aligned(const char *file, int line, const char *func,
    void *start, size_t size);

/* a malloc() that can't return NULL */
#define MALLOC(size)\
    ut_malloc(__FILE__, __LINE__, __func__, size)

/* a calloc() that can't return NULL */
#define CALLOC(nmemb, size)\
    ut_calloc(__FILE__, __LINE__, __func__, nmemb, size)

/* a malloc() of zeroed memory */
#define ZALLOC(size)\
    ut_calloc(__FILE__, __LINE__, __func__, 1, size)

#define FREE(ptr)\
    ut_free(__FILE__, __LINE__, __func__, ptr)

#define ALIGNED_FREE(ptr)\
    ut_aligned_free(__FILE__, __LINE__, __func__, ptr)

/* a realloc() that can't return NULL */
#define REALLOC(ptr, size)\
    ut_realloc(__FILE__, __LINE__, __func__, ptr, size)

/* a strdup() that can't return NULL */
#define STRDUP(str)\
    ut_strdup(__FILE__, __LINE__, __func__, str)

/* a malloc() that only returns page aligned memory */
#define PAGEALIGNMALLOC(size)\
    ut_pagealignmalloc(__FILE__, __LINE__, __func__, size)

/* a malloc() that returns memory with given alignment */
#define MEMALIGN(alignment, size)\
    ut_memalign(__FILE__, __LINE__, __func__, alignment, size)

/*
 * A mmap() that returns anonymous memory with given alignment and guard
 * pages.
 */
#define MMAP_ANON_ALIGNED(size, alignment)\
    ut_mmap_anon_aligned(__FILE__, __LINE__, __func__, alignment, size)

#define MUNMAP_ANON_ALIGNED(start, size)\
    ut_munmap_anon_aligned(__FILE__, __LINE__, __func__, start, size)

/*
 * file operations
 */
int ut_open(const char *file, int line, const char *func, const char *path,
    int flags, ...);

int ut_wopen(const char *file, int line, const char *func, const wchar_t *path,
	int flags, ...);

int ut_close(const char *file, int line, const char *func, int fd);

FILE *ut_fopen(const char *file, int line, const char *func, const char *path,
    const char *mode);

int ut_fclose(const char *file, int line, const char *func, FILE *stream);

int ut_unlink(const char *file, int line, const char *func, const char *path);

size_t ut_write(const char *file, int line, const char *func, int fd,
    const void *buf, size_t len);

size_t ut_read(const char *file, int line, const char *func, int fd,
    void *buf, size_t len);

os_off_t ut_lseek(const char *file, int line, const char *func, int fd,
    os_off_t offset, int whence);

int ut_posix_fallocate(const char *file, int line, const char *func, int fd,
    os_off_t offset, os_off_t len);

int ut_stat(const char *file, int line, const char *func, const char *path,
    os_stat_t *st_bufp);

int ut_fstat(const char *file, int line, const char *func, int fd,
    os_stat_t *st_bufp);

void *ut_mmap(const char *file, int line, const char *func, void *addr,
    size_t length, int prot, int flags, int fd, os_off_t offset);

int ut_munmap(const char *file, int line, const char *func, void *addr,
    size_t length);

int ut_mprotect(const char *file, int line, const char *func, void *addr,
    size_t len, int prot);

int ut_ftruncate(const char *file, int line, const char *func,
    int fd, os_off_t length);

void *ut_file_map(const char *file, int line, const char *func, int fd,
    size_t size);

long long ut_strtoll(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

long ut_strtol(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

int ut_strtoi(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

unsigned long long ut_strtoull(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

unsigned long ut_strtoul(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

unsigned ut_strtou(const char *file, int line, const char *func,
    const char *nptr, char **endptr, int base);

int ut_snprintf(const char *file, int line, const char *func,
		char *str, size_t size, const char *format, ...);

/* an open() that can't return < 0 */
#define OPEN(path, ...)\
    ut_open(__FILE__, __LINE__, __func__, path, __VA_ARGS__)

/* a _wopen() that can't return < 0 */
#define WOPEN(path, ...)\
    ut_wopen(__FILE__, __LINE__, __func__, path, __VA_ARGS__)

/* a close() that can't return -1 */
#define CLOSE(fd)\
    ut_close(__FILE__, __LINE__, __func__, fd)

/* an fopen() that can't return != 0 */
#define FOPEN(path, mode)\
    ut_fopen(__FILE__, __LINE__, __func__, path, mode)

/* a fclose() that can't return != 0 */
#define FCLOSE(stream)\
    ut_fclose(__FILE__, __LINE__, __func__, stream)

/* an unlink() that can't return -1 */
#define UNLINK(path)\
    ut_unlink(__FILE__, __LINE__, __func__, path)

/* a write() that can't return -1 */
#define WRITE(fd, buf, len)\
    ut_write(__FILE__, __LINE__, __func__, fd, buf, len)

/* a read() that can't return -1 */
#define READ(fd, buf, len)\
    ut_read(__FILE__, __LINE__, __func__, fd, buf, len)

/* a lseek() that can't return -1 */
#define LSEEK(fd, offset, whence)\
    ut_lseek(__FILE__, __LINE__, __func__, fd, offset, whence)

#define POSIX_FALLOCATE(fd, off, len)\
    ut_posix_fallocate(__FILE__, __LINE__, __func__, fd, off, len)

#define FSTAT(fd, st_bufp)\
    ut_fstat(__FILE__, __LINE__, __func__, fd, st_bufp)

/* a mmap() that can't return MAP_FAILED */
#define MMAP(addr, len, prot, flags, fd, offset)\
    ut_mmap(__FILE__, __LINE__, __func__, addr, len, prot, flags, fd, offset);

/* a munmap() that can't return -1 */
#define MUNMAP(addr, length)\
    ut_munmap(__FILE__, __LINE__, __func__, addr, length);

/* a mprotect() that can't return -1 */
#define MPROTECT(addr, len, prot)\
    ut_mprotect(__FILE__, __LINE__, __func__, addr, len, prot);

#define STAT(path, st_bufp)\
    ut_stat(__FILE__, __LINE__, __func__, path, st_bufp)

#define FTRUNCATE(fd, length)\
    ut_ftruncate(__FILE__, __LINE__, __func__, fd, length)

#define FILE_MAP(fd, size)\
    ut_file_map(__FILE__, __LINE__, __func__, fd, size);

#define ATOU(nptr) STRTOU(nptr, NULL, 10)
#define ATOUL(nptr) STRTOUL(nptr, NULL, 10)
#define ATOULL(nptr) STRTOULL(nptr, NULL, 10)
#define ATOI(nptr) STRTOI(nptr, NULL, 10)
#define ATOL(nptr) STRTOL(nptr, NULL, 10)
#define ATOLL(nptr) STRTOLL(nptr, NULL, 10)

#define STRTOULL(nptr, endptr, base)\
    ut_strtoull(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOUL(nptr, endptr, base)\
    ut_strtoul(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOL(nptr, endptr, base)\
    ut_strtol(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOLL(nptr, endptr, base)\
    ut_strtoll(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOU(nptr, endptr, base)\
    ut_strtou(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define STRTOI(nptr, endptr, base)\
    ut_strtoi(__FILE__, __LINE__, __func__, nptr, endptr, base)

#define SNPRINTF(str, size, format, ...) \
	ut_snprintf(__FILE__, __LINE__, __func__, \
			str, size, format, __VA_ARGS__)

#define ut_jmp_buf_t sigjmp_buf
#define ut_siglongjmp(b) siglongjmp(b, 1)
#define ut_sigsetjmp(b) sigsetjmp(b, 1)

void ut_suppress_errmsg(void);
void ut_unsuppress_errmsg(void);
void ut_suppress_crt_assert(void);
void ut_unsuppress_crt_assert(void);
/*
 * signals...
 */
int ut_sigaction(const char *file, int line, const char *func,
    int signum, struct sigaction *act, struct sigaction *oldact);

/* a sigaction() that can't return an error */
#define SIGACTION(signum, act, oldact)\
    ut_sigaction(__FILE__, __LINE__, __func__, signum, act, oldact)

/*
 * pthreads...
 */
int ut_thread_create(const char *file, int line, const char *func,
    os_thread_t *__restrict thread,
    const os_thread_attr_t *__restrict attr,
    void *(*start_routine)(void *), void *__restrict arg);
int ut_thread_join(const char *file, int line, const char *func,
    os_thread_t *thread, void **value_ptr);

/* a os_thread_create() that can't return an error */
#define THREAD_CREATE(thread, attr, start_routine, arg)\
    ut_thread_create(__FILE__, __LINE__, __func__,\
    thread, attr, start_routine, arg)

/* a os_thread_join() that can't return an error */
#define THREAD_JOIN(thread, value_ptr)\
    ut_thread_join(__FILE__, __LINE__, __func__, thread, value_ptr)

/*
 * mocks...
 *
 * NOTE: On Linux, function mocking is implemented using wrapper functions.
 * See "--wrap" option of the GNU linker.
 */
#define _FUNC_REAL_DECL(name, ret_type, ...)\
	ret_type __real_##name(__VA_ARGS__) __attribute__((unused));

#define _FUNC_REAL(name)\
	__real_##name

#define RCOUNTER(name)\
	_rcounter##name

#define FUNC_MOCK_RCOUNTER_SET(name, val)\
    RCOUNTER(name) = val;

#define FUNC_MOCK(name, ret_type, ...)\
	_FUNC_REAL_DECL(name, ret_type, ##__VA_ARGS__)\
	static unsigned RCOUNTER(name);\
	ret_type __wrap_##name(__VA_ARGS__);\
	ret_type __wrap_##name(__VA_ARGS__) {\
		switch (util_fetch_and_add32(&RCOUNTER(name), 1)) {

#define FUNC_MOCK_DLLIMPORT(name, ret_type, ...)\
	__declspec(dllimport) _FUNC_REAL_DECL(name, ret_type, ##__VA_ARGS__)\
	static unsigned RCOUNTER(name);\
	ret_type __wrap_##name(__VA_ARGS__);\
	ret_type __wrap_##name(__VA_ARGS__) {\
		switch (util_fetch_and_add32(&RCOUNTER(name), 1)) {

#define FUNC_MOCK_END\
	}}

#define FUNC_MOCK_RUN(run)\
	case run:

#define FUNC_MOCK_RUN_DEFAULT\
	default:

#define FUNC_MOCK_RUN_RET(run, ret)\
	case run: return (ret);

#define FUNC_MOCK_RUN_RET_DEFAULT_REAL(name, ...)\
	default: return _FUNC_REAL(name)(__VA_ARGS__);

#define FUNC_MOCK_RUN_RET_DEFAULT(ret)\
	default: return (ret);

#define FUNC_MOCK_RET_ALWAYS(name, ret_type, ret, ...)\
	FUNC_MOCK(name, ret_type, __VA_ARGS__)\
		FUNC_MOCK_RUN_RET_DEFAULT(ret);\
	FUNC_MOCK_END

#define FUNC_MOCK_RET_ALWAYS_VOID(name, ...)\
	FUNC_MOCK(name, void, __VA_ARGS__)\
		default: return;\
	FUNC_MOCK_END

extern unsigned long Ut_pagesize;
extern unsigned long long Ut_mmap_align;
extern os_mutex_t Sigactions_lock;

void ut_dump_backtrace(void);
void ut_sighandler(int);
void ut_register_sighandlers(void);

uint16_t ut_checksum(uint8_t *addr, size_t len);
char *ut_toUTF8(const wchar_t *wstr);
wchar_t *ut_toUTF16(const char *wstr);

struct test_case {
	const char *name;
	int (*func)(const struct test_case *tc, int argc, char *argv[]);
};

/*
 * get_tc -- return test case of specified name
 */
static inline const struct test_case *
get_tc(const char *name, const struct test_case *test_cases, size_t ntests)
{
	for (size_t i = 0; i < ntests; i++) {
		if (strcmp(name, test_cases[i].name) == 0)
			return &test_cases[i];
	}

	return NULL;
}

static inline void
TEST_CASE_PROCESS(int argc, char *argv[],
	const struct test_case *test_cases, size_t ntests)
{
	if (argc < 2)
		UT_FATAL("usage: %s <test case> [<args>]", argv[0]);

	for (int i = 1; i < argc; i++) {
		char *str_test = argv[i];
		const int args_off = i + 1;

		const struct test_case *tc = get_tc(str_test,
				test_cases, ntests);
		if (!tc)
			UT_FATAL("unknown test case -- '%s'", str_test);

		int ret = tc->func(tc, argc - args_off, &argv[args_off]);
		if (ret < 0)
			UT_FATAL("test return value cannot be negative");

		i += ret;
	}
}

#define TEST_CASE_DECLARE(_name)\
int \
_name(const struct test_case *tc, int argc, char *argv[])

#define TEST_CASE(_name)\
{\
	.name = #_name,\
	.func = (_name),\
}

#define STR(x) #x

#define ASSERT_ALIGNED_BEGIN(type) do {\
size_t off = 0;\
const char *last = "(none)";\
type t;

#define ASSERT_ALIGNED_FIELD(type, field) do {\
if (offsetof(type, field) != off)\
	UT_FATAL("%s: padding, missing field or fields not in order between "\
		"'%s' and '%s' -- offset %lu, real offset %lu",\
		STR(type), last, STR(field), off, offsetof(type, field));\
off += sizeof(t.field);\
last = STR(field);\
} while (0)

#define ASSERT_FIELD_SIZE(field, size) do {\
UT_COMPILE_ERROR_ON(size != sizeof(t.field));\
} while (0)

#define ASSERT_OFFSET_CHECKPOINT(type, checkpoint) do {\
if (off != checkpoint)\
	UT_FATAL("%s: violated offset checkpoint -- "\
		"checkpoint %lu, real offset %lu",\
		STR(type), checkpoint, off);\
} while (0)

#define ASSERT_ALIGNED_CHECK(type)\
if (off != sizeof(type))\
	UT_FATAL("%s: missing field or padding after '%s': "\
		"sizeof(%s) = %lu, fields size = %lu",\
		STR(type), last, STR(type), sizeof(type), off);\
} while (0)

struct markers {
	size_t markers_no;
	char **markers;
};

/*
 * get_markers - parse markers from pmreorder
 *		(which are passed in the form of char array
 *		e.g."MARKER.BEGIN|MARKER.END")
 *		and return struct including list of markers
 *		and information about its quantity
 */
static inline struct markers *
get_markers(const char *input)
{
	if (!input)
		return NULL;

	struct markers *log = (struct markers *)MALLOC(sizeof(struct markers));
	const char *delim = "|";
	char *tmp = STRDUP(input);

	log->markers_no = 1;
	for (char *s = tmp; *s != '\0'; s++)
		if (*s == *delim)
			log->markers_no++;
	log->markers = (char **)MALLOC(log->markers_no * sizeof(char *));

	unsigned i = 0;
	char *token;
	char *rest = NULL;

	for (token = strtok_r(tmp, delim, &rest); token != NULL;
		token = strtok_r(NULL, delim, &rest)) {
		log->markers[i] = STRDUP(token);
		i++;
	}

	FREE(tmp);
	return log;
}

static inline void
delete_markers(struct markers *log)
{
	for (size_t i = 0; i < log->markers_no; i++)
		FREE(log->markers[i]);
	FREE(log->markers);
	FREE(log);
}

/*
 * AddressSanitizer
 */
#ifdef __clang__
#if __has_feature(address_sanitizer)
#define UT_DEFINE_ASAN_POISON
#endif
#else
#ifdef __SANITIZE_ADDRESS__
#define UT_DEFINE_ASAN_POISON
#endif
#endif
#ifdef UT_DEFINE_ASAN_POISON
void __asan_poison_memory_region(void const volatile *addr, size_t size);
void __asan_unpoison_memory_region(void const volatile *addr, size_t size);
#define ASAN_POISON_MEMORY_REGION(addr, size) \
	__asan_poison_memory_region((addr), (size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
	__asan_unpoison_memory_region((addr), (size))
#else
#define ASAN_POISON_MEMORY_REGION(addr, size) \
	((void)(addr), (void)(size))
#define ASAN_UNPOISON_MEMORY_REGION(addr, size) \
	((void)(addr), (void)(size))
#endif

#ifdef __cplusplus
}
#endif

#endif	/* unittest.h */
