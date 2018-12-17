/*
 * Copyright 2014-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * util.c -- very basic utilities
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>
#include <time.h>

#include "util.h"
#include "valgrind_internal.h"

/* library-wide page size */
unsigned long long Pagesize;

/* allocation/mmap granularity */
unsigned long long Mmap_align;

/*
 * our versions of malloc & friends start off pointing to the libc versions
 */
Malloc_func Malloc = malloc;
Free_func Free = free;
Realloc_func Realloc = realloc;
Strdup_func Strdup = strdup;

/*
 * Zalloc -- allocate zeroed memory
 */
void *
Zalloc(size_t sz)
{
	void *ret = Malloc(sz);
	if (!ret)
		return NULL;
	return memset(ret, 0, sz);
}

#if ANY_VG_TOOL_ENABLED
/* initialized to true if the process is running inside Valgrind */
unsigned _On_valgrind;
#endif

#if VG_PMEMCHECK_ENABLED
#define LIB_LOG_LEN 20
#define FUNC_LOG_LEN 50
#define SUFFIX_LEN 7

/* true if pmreorder instrumentization has to be enabled */
int _Pmreorder_emit;

/*
 * util_emit_log -- emits lib and func name with appropriate suffix
 * to pmemcheck store log file
 */
void
util_emit_log(const char *lib, const char *func, int order)
{
	char lib_name[LIB_LOG_LEN];
	char func_name[FUNC_LOG_LEN];
	char suffix[SUFFIX_LEN];
	size_t lib_len = strlen(lib);
	size_t func_len = strlen(func);

	if (order == 0)
		strcpy(suffix, ".BEGIN");
	else
		strcpy(suffix, ".END");

	size_t suffix_len = strlen(suffix);

	if (lib_len + suffix_len + 1 > LIB_LOG_LEN) {
		VALGRIND_EMIT_LOG("Library name is too long");
		return;
	}

	if (func_len + suffix_len + 1 > FUNC_LOG_LEN) {
		VALGRIND_EMIT_LOG("Function name is too long");
		return;
	}

	strcpy(lib_name, lib);
	strcat(lib_name, suffix);
	strcpy(func_name, func);
	strcat(func_name, suffix);

	if (order == 0) {
		VALGRIND_EMIT_LOG(func_name);
		VALGRIND_EMIT_LOG(lib_name);
	} else {
		VALGRIND_EMIT_LOG(lib_name);
		VALGRIND_EMIT_LOG(func_name);
	}
}
#endif

/*
 * util_is_zeroed -- check if given memory range is all zero
 */
int
util_is_zeroed(const void *addr, size_t len)
{
	const char *a = addr;

	if (len == 0)
		return 1;

	if (a[0] == 0 && memcmp(a, a + 1, len - 1) == 0)
		return 1;

	return 0;
}

/*
 * util_checksum -- compute Fletcher64 checksum
 *
 * csump points to where the checksum lives, so that location
 * is treated as zeros while calculating the checksum. The
 * checksummed data is assumed to be in little endian order.
 * If insert is true, the calculated checksum is inserted into
 * the range at *csump.  Otherwise the calculated checksum is
 * checked against *csump and the result returned (true means
 * the range checksummed correctly).
 */
int
util_checksum(void *addr, size_t len, uint64_t *csump,
	int insert, size_t skip_off)
{
	if (len % 4 != 0)
		abort();

	uint32_t *p32 = addr;
	uint32_t *p32end = (uint32_t *)((char *)addr + len);
	uint32_t *skip;
	uint32_t lo32 = 0;
	uint32_t hi32 = 0;
	uint64_t csum;

	if (skip_off)
		skip = (uint32_t *)((char *)addr + skip_off);
	else
		skip = (uint32_t *)((char *)addr + len);

	while (p32 < p32end)
		if (p32 == (uint32_t *)csump || p32 >= skip) {
			/* lo32 += 0; treat first 32-bits as zero */
			p32++;
			hi32 += lo32;
			/* lo32 += 0; treat second 32-bits as zero */
			p32++;
			hi32 += lo32;
		} else {
			lo32 += le32toh(*p32);
			++p32;
			hi32 += lo32;
		}

	csum = (uint64_t)hi32 << 32 | lo32;

	if (insert) {
		*csump = htole64(csum);
		return 1;
	}

	return *csump == htole64(csum);
}

/*
 * util_checksum_seq -- compute sequential Fletcher64 checksum
 *
 * Merges checksum from the old buffer with checksum for current buffer.
 */
uint64_t
util_checksum_seq(const void *addr, size_t len, uint64_t csum)
{
	if (len % 4 != 0)
		abort();
	const uint32_t *p32 = addr;
	const uint32_t *p32end = (const uint32_t *)((const char *)addr + len);
	uint32_t lo32 = (uint32_t)csum;
	uint32_t hi32 = (uint32_t)(csum >> 32);
	while (p32 < p32end) {
		lo32 += le32toh(*p32);
		++p32;
		hi32 += lo32;
	}
	return (uint64_t)hi32 << 32 | lo32;
}

/*
 * util_set_alloc_funcs -- allow one to override malloc, etc.
 */
void
util_set_alloc_funcs(void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	Malloc = (malloc_func == NULL) ? malloc : malloc_func;
	Free = (free_func == NULL) ? free : free_func;
	Realloc = (realloc_func == NULL) ? realloc : realloc_func;
	Strdup = (strdup_func == NULL) ? strdup : strdup_func;
}

/*
 * util_fgets -- fgets wrapper with conversion CRLF to LF
 */
char *
util_fgets(char *buffer, int max, FILE *stream)
{
	char *str = fgets(buffer, max, stream);
	if (str == NULL)
		goto end;

	int len = (int)strlen(str);
	if (len < 2)
		goto end;
	if (str[len - 2] == '\r' && str[len - 1] == '\n') {
		str[len - 2] = '\n';
		str[len - 1] = '\0';
	}
end:
	return str;
}

struct suff {
	const char *suff;
	uint64_t mag;
};

/*
 * util_parse_size -- parse size from string
 */
int
util_parse_size(const char *str, size_t *sizep)
{
	const struct suff suffixes[] = {
		{ "B", 1ULL },
		{ "K", 1ULL << 10 },		/* JEDEC */
		{ "M", 1ULL << 20 },
		{ "G", 1ULL << 30 },
		{ "T", 1ULL << 40 },
		{ "P", 1ULL << 50 },
		{ "KiB", 1ULL << 10 },		/* IEC */
		{ "MiB", 1ULL << 20 },
		{ "GiB", 1ULL << 30 },
		{ "TiB", 1ULL << 40 },
		{ "PiB", 1ULL << 50 },
		{ "kB", 1000ULL },		/* SI */
		{ "MB", 1000ULL * 1000 },
		{ "GB", 1000ULL * 1000 * 1000 },
		{ "TB", 1000ULL * 1000 * 1000 * 1000 },
		{ "PB", 1000ULL * 1000 * 1000 * 1000 * 1000 }
	};

	int res = -1;
	unsigned i;
	size_t size = 0;
	char unit[9] = {0};

	int ret = sscanf(str, "%zu%8s", &size, unit);
	if (ret == 1) {
		res = 0;
	} else if (ret == 2) {
		for (i = 0; i < ARRAY_SIZE(suffixes); ++i) {
			if (strcmp(suffixes[i].suff, unit) == 0) {
				size = size * suffixes[i].mag;
				res = 0;
				break;
			}
		}
	} else {
		return -1;
	}

	if (sizep && res == 0)
		*sizep = size;
	return res;
}

/*
 * util_init -- initialize the utils
 *
 * This is called from the library initialization code.
 */
void
util_init(void)
{
	/* XXX - replace sysconf() with util_get_sys_xxx() */
	if (Pagesize == 0)
		Pagesize = (unsigned long) sysconf(_SC_PAGESIZE);

#ifndef _WIN32
	Mmap_align = Pagesize;
#else
	if (Mmap_align == 0) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		Mmap_align = si.dwAllocationGranularity;
	}
#endif

#if ANY_VG_TOOL_ENABLED
	_On_valgrind = RUNNING_ON_VALGRIND;
#endif

#if VG_PMEMCHECK_ENABLED
	if (On_valgrind) {
		char *pmreorder_env = getenv("PMREORDER_EMIT_LOG");
		if (pmreorder_env)
			_Pmreorder_emit = atoi(pmreorder_env);
	} else {
			_Pmreorder_emit = 0;
	}
#endif
}

/*
 * util_concat_str -- concatenate two strings
 */
char *
util_concat_str(const char *s1, const char *s2)
{
	char *result = malloc(strlen(s1) + strlen(s2) + 1);
	if (!result)
		return NULL;

	strcpy(result, s1);
	strcat(result, s2);

	return result;
}

/*
 * util_localtime -- a wrapper for localtime function
 *
 * localtime can set nonzero errno even if it succeeds (e.g. when there is no
 * /etc/localtime file under Linux) and we do not want the errno to be polluted
 * in such cases.
 */
struct tm *
util_localtime(const time_t *timep)
{
	int oerrno = errno;
	struct tm *tm = localtime(timep);
	if (tm != NULL)
		errno = oerrno;

	return tm;
}

/*
 * util_safe_strcpy -- copies string from src to dst, returns -1
 * when length of source string (including null-terminator)
 * is greater than max_length, 0 otherwise
 *
 * For gcc (found in version 8.1.1) calling this function with
 * max_length equal to dst size produces -Wstringop-truncation warning
 *
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=85902
 */
#ifdef STRINGOP_TRUNCATION_SUPPORTED
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
int
util_safe_strcpy(char *dst, const char *src, size_t max_length)
{
	if (max_length == 0)
		return -1;

	strncpy(dst, src, max_length);

	return dst[max_length - 1] == '\0' ? 0 : -1;
}
#ifdef STRINGOP_TRUNCATION_SUPPORTED
#pragma GCC diagnostic pop
#endif

#define PARSER_MAX_LINE (PATH_MAX + 1024)

/*
 * util_readline -- read line from stream
 */
char *
util_readline(FILE *fh)
{
	size_t bufsize = PARSER_MAX_LINE;
	size_t position = 0;
	char *buffer = NULL;

	do {
		char *tmp = buffer;
		buffer = Realloc(buffer, bufsize);
		if (buffer == NULL) {
			Free(tmp);
			return NULL;
		}

		/* ensure if we can cast bufsize to int */
		char *s = util_fgets(buffer + position, (int)bufsize / 2, fh);
		if (s == NULL) {
			Free(buffer);
			return NULL;
		}

		position = strlen(buffer);
		bufsize *= 2;
	} while (!feof(fh) && buffer[position - 1] != '\n');

	return buffer;
}
