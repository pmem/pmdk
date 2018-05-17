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
 * vec.h -- vector interface
 */

#ifndef PMDK_VEC_H
#define PMDK_VEC_H 1

#include <stddef.h>
#include "util.h"

#define VEC_GROW_SIZE (64)

#define VEC(name, type)\
struct name {\
	type *buffer;\
	size_t size;\
	size_t capacity;\
}

#define VEC_INITIALIZER {NULL, 0, 0}

#define VEC_INIT(vec) do {\
	(vec)->buffer = NULL;\
	(vec)->size = 0;\
	(vec)->capacity = 0;\
} while (0)

static inline int
vec_reserve(void *vec, size_t ncapacity, size_t s)
{
	VEC(vvec, void) *vecp = (struct vvec *)vec;
	void *tbuf = Realloc(vecp->buffer, s * ncapacity);
	if (tbuf == NULL)
		return -1;
	vecp->buffer = tbuf;
	vecp->capacity = ncapacity;
	return 0;
}

#define VEC_RESERVE(vec, ncapacity)\
((ncapacity) > (vec)->size ?\
	vec_reserve((void *)vec, ncapacity, sizeof(*(vec)->buffer)) :\
	0)

#define VEC_POP_BACK(vec) do {\
	(vec)->size -= 1;\
} while (0)

#define VEC_FRONT(vec)\
(vec)->buffer[0]

#define VEC_BACK(vec)\
(vec)->buffer[(vec)->size - 1]

#define VEC_ERASE_BY_POS(vec, pos) do {\
	(vec)->buffer[(pos)] = VEC_BACK(vec);\
	VEC_POP_BACK(vec);\
} while (0)

#define VEC_ERASE_BY_PTR(vec, element) do {\
	ptrdiff_t elpos = (uintptr_t)(element) - (uintptr_t)((vec)->buffer);\
	elpos /= sizeof(*element);\
	VEC_ERASE_BY_POS(vec, elpos);\
} while (0)

#define VEC_INSERT(vec, element)\
((vec)->buffer[(vec)->size++] = (element), 0)

#define VEC_PUSH_BACK(vec, element)\
((vec)->capacity == (vec)->size ?\
	(VEC_RESERVE((vec), ((vec)->capacity + VEC_GROW_SIZE)) == 0 ?\
		VEC_INSERT(vec, element) : -1) :\
	VEC_INSERT(vec, element))

#define VEC_FOREACH(el, vec)\
for (size_t _vec_i = 0;\
	_vec_i < (vec)->size && ((el = (vec)->buffer[_vec_i]), 1);\
	++_vec_i)

#define VEC_FOREACH_BY_POS(elpos, vec)\
for (elpos = 0; elpos < (vec)->size; ++elpos)

#define VEC_FOREACH_BY_PTR(el, vec)\
for (size_t _vec_i = 0;\
	_vec_i < (vec)->size && ((el = &(vec)->buffer[_vec_i]), 1);\
	++_vec_i)

#define VEC_SIZE(vec)\
((vec)->size)

#define VEC_CAPACITY(vec)\
((vec)->capacity)

#define VEC_ARR(vec)\
((vec)->buffer)

#define VEC_GET(vec, id)\
(&(vec)->buffer[id])

#define VEC_CLEAR(vec) do {\
	(vec)->size = 0;\
} while (0)

#define VEC_DELETE(vec) do {\
	Free((vec)->buffer);\
} while (0)

#endif /* PMDK_VEC_H */
