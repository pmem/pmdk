/*
 * Copyright 2015-2017, Intel Corporation
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
 * mocks-linux.c -- redefinitions of locks function
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
