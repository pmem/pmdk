/*
 * Copyright 2016, Intel Corporation
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
 * rpmemd_log.h -- rpmemd logging functions declarations
 */

#define FORMAT_PRINTF(a, b) __attribute__((__format__(__printf__, (a), (b))))

#ifdef DEBUG
#define RPMEMD_LOG(level, fmt, arg...)\
	rpmemd_log(RPD_LOG_##level, __FILE__, __LINE__, fmt, ## arg)
#else
#define RPMEMD_LOG(level, fmt, arg...)\
	rpmemd_log(RPD_LOG_##level, NULL, 0, fmt, ## arg)
#endif

#ifdef DEBUG
#define RPMEMD_DBG(fmt, arg...)\
	rpmemd_log(_RPD_LOG_DBG, __FILE__, __LINE__, fmt, ## arg)
#else
#define RPMEMD_DBG(fmt, arg...) do {} while (0)
#endif

#define RPMEMD_FATAL(fmt, arg...) do {\
	RPMEMD_LOG(ERR, fmt, ## arg);\
	abort();\
} while (0)

#define RPMEMD_ASSERT(cond) do {\
	if (!(cond)) {\
		rpmemd_log(RPD_LOG_ERR, __FILE__, __LINE__,\
			"assertion fault: %s", #cond);\
		abort();\
	}\
} while (0)

enum rpmemd_log_level {
	RPD_LOG_ERR,
	RPD_LOG_WARN,
	RPD_LOG_NOTICE,
	RPD_LOG_INFO,
	_RPD_LOG_DBG,	/* disallow to use this with LOG macro */
	MAX_RPD_LOG,
};

enum rpmemd_log_level rpmemd_log_level_from_str(const char *str);
const char *rpmemd_log_level_to_str(enum rpmemd_log_level level);

extern enum rpmemd_log_level rpmemd_log_level;
int rpmemd_log_init(const char *ident, const char *fname, int use_syslog);
void rpmemd_log_close(void);
int rpmemd_prefix(const char *fmt, ...) FORMAT_PRINTF(1, 2);
void rpmemd_log(enum rpmemd_log_level level, const char *fname,
		int lineno, const char *fmt, ...) FORMAT_PRINTF(4, 5);
