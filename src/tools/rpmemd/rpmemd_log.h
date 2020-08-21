/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * rpmemd_log.h -- rpmemd logging functions declarations
 */

#include <string.h>
#include "util.h"

#define FORMAT_PRINTF(a, b) __attribute__((__format__(__printf__, (a), (b))))

/*
 * The tab character is not allowed in rpmemd log,
 * because it is not well handled by syslog.
 * Please use RPMEMD_LOG_INDENT instead.
 */
#define RPMEMD_LOG_INDENT "    "

#ifdef DEBUG
#define RPMEMD_LOG(level, fmt, arg...) do {\
	COMPILE_ERROR_ON(strchr(fmt, '\t') != 0);\
	rpmemd_log(RPD_LOG_##level, __FILE__, __LINE__, fmt, ## arg);\
} while (0)
#else
#define RPMEMD_LOG(level, fmt, arg...) do {\
	COMPILE_ERROR_ON(strchr(fmt, '\t') != 0);\
	rpmemd_log(RPD_LOG_##level, NULL, 0, fmt, ## arg);\
} while (0)
#endif

#ifdef DEBUG
#define RPMEMD_DBG(fmt, arg...) do {\
	COMPILE_ERROR_ON(strchr(fmt, '\t') != 0);\
	rpmemd_log(_RPD_LOG_DBG, __FILE__, __LINE__, fmt, ## arg);\
} while (0)
#else
#define RPMEMD_DBG(fmt, arg...) do {} while (0)
#endif

#define RPMEMD_ERR(fmt, arg...) do {\
	RPMEMD_LOG(ERR, fmt, ## arg);\
} while (0)

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
