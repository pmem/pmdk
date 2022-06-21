// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * ut_pthread.c -- unit test wrappers for pthread routines
 */

#include "unittest.h"

/*
 * ut_thread_create -- a os_thread_create that cannot return an error
 */
int
ut_thread_create(const char *file, int line, const char *func,
    os_thread_t *__restrict thread,
    const os_thread_attr_t *__restrict attr,
    void *(*start_routine)(void *), void *__restrict arg)
{
	if ((errno = os_thread_create(thread, attr, start_routine, arg)) != 0)
		ut_fatal(file, line, func, "!os_thread_create");

	return 0;
}

/*
 * ut_thread_join -- a os_thread_join that cannot return an error
 */
int
ut_thread_join(const char *file, int line, const char *func,
    os_thread_t *thread, void **value_ptr)
{
	if ((errno = os_thread_join(thread, value_ptr)) != 0)
		ut_fatal(file, line, func, "!os_thread_join");

	return 0;
}
