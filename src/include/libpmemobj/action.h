/*
 * Copyright 2017-2018, Intel Corporation
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
 * libpmemobj/action.h -- definitions of libpmemobj action interface
 */

#ifndef LIBPMEMOBJ_ACTION_H
#define LIBPMEMOBJ_ACTION_H 1

#include <libpmemobj/action_base.h>

#ifdef __cplusplus
extern "C" {
#endif

#define POBJ_RESERVE_NEW(pop, t, act)\
((TOID(t))pmemobj_reserve(pop, act, sizeof(t), TOID_TYPE_NUM(t)))

#define POBJ_RESERVE_ALLOC(pop, t, size, act)\
((TOID(t))pmemobj_reserve(pop, act, size, TOID_TYPE_NUM(t)))

#define POBJ_XRESERVE_NEW(pop, t, act, flags)\
((TOID(t))pmemobj_xreserve(pop, act, sizeof(t), TOID_TYPE_NUM(t), flags))

#define POBJ_XRESERVE_ALLOC(pop, t, size, act, flags)\
((TOID(t))pmemobj_xreserve(pop, act, size, TOID_TYPE_NUM(t), flags))

#ifdef __cplusplus
}
#endif

#endif /* libpmemobj/action_base.h */
