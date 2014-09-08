/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * out.h -- definitions for "out" module
 */

#ifdef	DEBUG

/* produce debug/trace output */
#define	LOG(level, ...)\
	out_log(__FILE__, __LINE__, __func__, level, __VA_ARGS__)

/* produce debug/trace output without prefix and new line */
#define	LOG_NONL(level, ...)\
	out_nonl(level, __VA_ARGS__)

/* produce output and exit */
#define	FATAL(...)\
	out_fatal(__FILE__, __LINE__, __func__, __VA_ARGS__)

/* assert a condition is true */
#define	ASSERT(cnd)\
	((void)((cnd) || (out_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s", #cnd), 0)))

/* assertion with extra info printed if assertion fails */
#define	ASSERTinfo(cnd, info)\
	((void)((cnd) || (out_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (%s = %s)", #cnd, #info, info), 0)))

/* assert two integer values are equal */
#define	ASSERTeq(lhs, rhs)\
	((void)(((lhs) == (rhs)) || (out_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) == %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

/* assert two integer values are not equal */
#define	ASSERTne(lhs, rhs)\
	((void)(((lhs) != (rhs)) || (out_fatal(__FILE__, __LINE__, __func__,\
	"assertion failure: %s (0x%llx) != %s (0x%llx)", #lhs,\
	(unsigned long long)(lhs), #rhs, (unsigned long long)(rhs)), 0)))

#else

/*
 * nondebug versions...
 */
#define	LOG(level, ...)
#define	LOG_NONL(level, ...)
#define	ASSERT(cnd)
#define	ASSERTinfo(cnd, info)
#define	ASSERTeq(lhs, rhs)
#define	ASSERTne(lhs, rhs)

/* shouldn't get called, but if it does, don't continue to run */
#define	FATAL(...) abort()

#endif	/* DEBUG */

void out_init(const char *log_prefix, const char *log_level_var,
		const char *log_file_var);
void out_fini();
void out(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void out_nonl(int level, const char *fmt,
	...) __attribute__((format(printf, 2, 3)));
void out_log(const char *file, int line, const char *func, int level,
	const char *fmt, ...)
	__attribute__((format(printf, 5, 6)));
void out_fatal(const char *file, int line, const char *func,
	const char *fmt, ...)
	__attribute__((format(printf, 4, 5)));
void out_set_print_func(void (*print_func)(const char *s));
