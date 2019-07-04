/*
 * Copyright 2018-2019, Intel Corporation
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
 * vecq.h -- vector queue (FIFO) interface
 */

#ifndef PMDK_VECQ_H
#define PMDK_VECQ_H 1

#include <stddef.h>
#include "util.h"
#include "out.h"
#include "alloc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define VECQ_INIT_SIZE (64)

#define VECQ(name, type)\
struct name {\
	type *buffer;\
	size_t capacity;\
	size_t front;\
	size_t back;\
}

#define VECQ_INIT(vec) do {\
	(vec)->buffer = NULL;\
	(vec)->capacity = 0;\
	(vec)->front = 0;\
	(vec)->back = 0;\
} while (0)

#define VECQ_REINIT(vec) do {\
	VALGRIND_ANNOTATE_NEW_MEMORY((vec), sizeof(*vec));\
	VALGRIND_ANNOTATE_NEW_MEMORY((vec)->buffer,\
		(sizeof(*(vec)->buffer) * ((vec)->capacity)));\
	(vec)->front = 0;\
	(vec)->back = 0;\
} while (0)

#define VECQ_FRONT_POS(vec)\
((vec)->front & ((vec)->capacity - 1))

#define VECQ_BACK_POS(vec)\
((vec)->back & ((vec)->capacity - 1))

#define VECQ_FRONT(vec)\
(vec)->buffer[VECQ_FRONT_POS(vec)]

#define VECQ_BACK(vec)\
(vec)->buffer[VECQ_BACK_POS(vec)]

#define VECQ_DEQUEUE(vec)\
((vec)->buffer[(((vec)->front++) & ((vec)->capacity - 1))])

#define VECQ_SIZE(vec)\
((vec)->back - (vec)->front)

static int __attribute((noinline))
vecq_grow(void *vec, size_t s)
{
	VECQ(vvec, void) *vecp = (struct vvec *)vec;
	size_t ncapacity = vecp->capacity == 0 ?
		VECQ_INIT_SIZE : vecp->capacity * 2;
	void *tbuf = Realloc(vecp->buffer, s * ncapacity);
	if (tbuf == NULL) {
		ERR("!Realloc");
		return -1;
	}
	memcpy((char *)tbuf + (s * vecp->capacity), (char *)tbuf,
		(s * VECQ_FRONT_POS(vecp)));

	vecp->front = VECQ_FRONT_POS(vecp);
	vecp->back = vecp->front + vecp->capacity;
	vecp->capacity = ncapacity;
	vecp->buffer = tbuf;

	return 0;
}

#define VECQ_GROW(vec)\
vecq_grow((void *)vec, sizeof(*(vec)->buffer))

#define VECQ_INSERT(vec, element)\
(VECQ_BACK(vec) = element, (vec)->back += 1, 0)

#define VECQ_ENQUEUE(vec, element)\
((vec)->capacity == VECQ_SIZE(vec) ?\
	(VECQ_GROW(vec) == 0 ? VECQ_INSERT(vec, element) : -1) :\
VECQ_INSERT(vec, element))

#define VECQ_CAPACITY(vec)\
((vec)->capacity)

#define VECQ_FOREACH(el, vec)\
for (size_t _vec_i = 0;\
	_vec_i < VECQ_SIZE(vec) &&\
	(((el) = (vec)->buffer[_vec_i & ((vec)->capacity - 1)]), 1);\
	++_vec_i)

#define VECQ_FOREACH_REVERSE(el, vec)\
for (size_t _vec_i = VECQ_SIZE(vec);\
	_vec_i > 0 &&\
	(((el) = (vec)->buffer[(_vec_i - 1) & ((vec)->capacity - 1)]), 1);\
	--_vec_i)

#define VECQ_CLEAR(vec) do {\
	(vec)->front = 0;\
	(vec)->back = 0;\
} while (0)

#define VECQ_DELETE(vec) do {\
	Free((vec)->buffer);\
	(vec)->buffer = NULL;\
	(vec)->capacity = 0;\
	(vec)->front = 0;\
	(vec)->back = 0;\
} while (0)

#ifdef __cplusplus
}
#endif

#endif /* PMDK_VECQ_H */
