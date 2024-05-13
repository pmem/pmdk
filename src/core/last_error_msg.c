// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * last_error_msg.c -- maintain TLS buffers to store the last error message
 *
 * The last error message is a hand-picked error message believed to convey
 * the critical piece of information which will be available to the user via
 * the *_errormsg() API calls.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "last_error_msg.h"
#include "os_thread.h"
#include "log_internal.h"
#include "valgrind_internal.h"

struct lasterrormsg
{
	char msg[CORE_LAST_ERROR_MSG_MAXPRINT];
};

static os_once_t Last_errormsg_key_once = OS_ONCE_INIT;
static os_tls_key_t Last_errormsg_key;

static void
last_error_msg_key_alloc(void)
{
	int pth_ret = os_tls_key_create(&Last_errormsg_key, free);
	if (pth_ret)
		CORE_LOG_FATAL_W_ERRNO("os_thread_key_create");

	VALGRIND_ANNOTATE_HAPPENS_BEFORE(&Last_errormsg_key_once);
}

void
last_error_msg_init(void)
{
	os_once(&Last_errormsg_key_once, last_error_msg_key_alloc);
	/*
	 * Workaround Helgrind's bug:
	 * https://bugs.kde.org/show_bug.cgi?id=337735
	 */
	VALGRIND_ANNOTATE_HAPPENS_AFTER(&Last_errormsg_key_once);
}

void
last_error_msg_fini(void)
{
	void *p = os_tls_get(Last_errormsg_key);
	if (p) {
		free(p);
		(void) os_tls_set(Last_errormsg_key, NULL);
	}
	(void) os_tls_key_delete(Last_errormsg_key);
}

static inline struct lasterrormsg *
last_error_msg_get_internal(void)
{
	last_error_msg_init();

	struct lasterrormsg *last = os_tls_get(Last_errormsg_key);
	if (last == NULL) {
		last = malloc(sizeof(struct lasterrormsg));
		if (last == NULL)
			return NULL;
		/* make sure it contains empty string initially */
		last->msg[0] = '\0';
		int ret = os_tls_set(Last_errormsg_key, last);
		if (ret)
			CORE_LOG_FATAL_W_ERRNO("os_tls_set");
	}
	return last;
}

/*
 * last_error_msg_get -- get the last error message
 */
const char *
last_error_msg_get(void)
{
	const struct lasterrormsg *last = last_error_msg_get_internal();
	return &last->msg[0];
}
