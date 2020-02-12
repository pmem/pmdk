// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

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

static inline int
realloc_set(void **buf, size_t s)
{
	void *tbuf = Realloc(*buf, s);
	if (tbuf == NULL) {
		ERR("!Realloc");
		return -1;
	}
	*buf = tbuf;
	return 0;
}

#define VECQ_NCAPACITY(vec)\
((vec)->capacity == 0 ? VECQ_INIT_SIZE : (vec)->capacity * 2)
#define VECQ_GROW(vec)\
(realloc_set((void **)&(vec)->buffer,\
		VECQ_NCAPACITY(vec) * sizeof(*(vec)->buffer)) ? -1 :\
	(memcpy((vec)->buffer + (vec)->capacity, (vec)->buffer,\
		VECQ_FRONT_POS(vec) * sizeof(*(vec)->buffer)),\
	(vec)->front = VECQ_FRONT_POS(vec),\
	(vec)->back = (vec)->front + (vec)->capacity,\
	(vec)->capacity = VECQ_NCAPACITY(vec),\
	0\
))

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
