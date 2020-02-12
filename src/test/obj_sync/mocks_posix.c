// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * mocks_posix.c -- redefinitions of lock functions (Posix implementation)
 */

#include <pthread.h>

#include "util.h"
#include "os.h"
#include "unittest.h"

FUNC_MOCK(pthread_mutex_init, int,
		pthread_mutex_t *__restrict mutex,
		const pthread_mutexattr_t *__restrict attr)
	FUNC_MOCK_RUN_RET_DEFAULT_REAL(pthread_mutex_init, mutex, attr)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END

FUNC_MOCK(pthread_rwlock_init, int,
		pthread_rwlock_t *__restrict rwlock,
		const pthread_rwlockattr_t *__restrict attr)
	FUNC_MOCK_RUN_RET_DEFAULT_REAL(pthread_rwlock_init, rwlock, attr)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END

FUNC_MOCK(pthread_cond_init, int,
		pthread_cond_t *__restrict cond,
		const pthread_condattr_t *__restrict attr)
	FUNC_MOCK_RUN_RET_DEFAULT_REAL(pthread_cond_init, cond, attr)
	FUNC_MOCK_RUN(1) {
		return -1;
	}
FUNC_MOCK_END
