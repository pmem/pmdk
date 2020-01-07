// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * mock-windows.c -- redefinitions of locks function
 */

#include "os.h"
#include "unittest.h"

FUNC_MOCK(os_mutex_init, int,
	os_mutex_t *__restrict mutex)

	FUNC_MOCK_RUN_RET_DEFAULT_REAL(os_mutex_init, mutex)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END

FUNC_MOCK(os_rwlock_init, int,
	os_rwlock_t *__restrict rwlock)

	FUNC_MOCK_RUN_RET_DEFAULT_REAL(os_rwlock_init, rwlock)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END

FUNC_MOCK(os_cond_init, int,
	os_cond_t *__restrict cond)

	FUNC_MOCK_RUN_RET_DEFAULT_REAL(os_cond_init, cond)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END
