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
