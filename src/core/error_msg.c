// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

/*
 * error_msg.c -- maintain TLS buffers to store the last error message
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

#include "error_msg.h"
#include "os_thread.h"
#include "out.h"
#include "valgrind_internal.h"

struct errormsg
{
	char msg[CORE_ERROR_MSG_MAXPRINT];
};

#ifndef NO_LIBPTHREAD

static os_once_t Last_errormsg_key_once = OS_ONCE_INIT;
static os_tls_key_t Last_errormsg_key;

static void
error_msg_key_alloc(void)
{
	int pth_ret = os_tls_key_create(&Last_errormsg_key, free);
	if (pth_ret)
		CORE_LOG_FATAL_W_ERRNO("os_thread_key_create");

	VALGRIND_ANNOTATE_HAPPENS_BEFORE(&Last_errormsg_key_once);
}

void
error_msg_init(void)
{
	os_once(&Last_errormsg_key_once, error_msg_key_alloc);
	/*
	 * Workaround Helgrind's bug:
	 * https://bugs.kde.org/show_bug.cgi?id=337735
	 */
	VALGRIND_ANNOTATE_HAPPENS_AFTER(&Last_errormsg_key_once);
}

void
error_msg_fini(void)
{
	void *p = os_tls_get(Last_errormsg_key);
	if (p) {
		free(p);
		(void) os_tls_set(Last_errormsg_key, NULL);
	}
	(void) os_tls_key_delete(Last_errormsg_key);
}

static inline struct errormsg *
error_msg_get_internal(void)
{
	error_msg_init();

	struct errormsg *errormsg = os_tls_get(Last_errormsg_key);
	if (errormsg == NULL) {
		errormsg = malloc(sizeof(struct errormsg));
		if (errormsg == NULL)
			return NULL;
		/* make sure it contains empty string initially */
		errormsg->msg[0] = '\0';
		int ret = os_tls_set(Last_errormsg_key, errormsg);
		if (ret)
			CORE_LOG_FATAL_W_ERRNO("os_tls_set");
	}
	return errormsg;
}

#else

/*
 * We don't want libpmem to depend on libpthread.  Instead of using pthread
 * API to dynamically allocate thread-specific error message buffer, we put
 * it into TLS.  However, keeping a pretty large static buffer (8K) in TLS
 * may lead to some issues, so the maximum message length is reduced.
 * Fortunately, it looks like the longest error message in libpmem should
 * not be longer than about 90 chars (in case of pmem_check_version()).
 */

static __thread struct errormsg Last_errormsg;

static inline void
error_msg_init(void)
{
}

static inline void
error_msg_fini(void)
{
}

static inline const struct errormsg *
error_msg_get_internal(void)
{
	return &Last_errormsg;
}

#endif /* NO_LIBPTHREAD */

/*
 * error_msg_get -- get the last error message
 */
const char *
error_msg_get(void)
{
	const struct errormsg *errormsg = error_msg_get_internal();
	return &errormsg->msg[0];
}
