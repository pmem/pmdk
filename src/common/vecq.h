/*
 * Copyright 2018, Intel Corporation
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

#define VECQ(name, type, nentries)\
struct name {\
	size_t capacity;\
	size_t front;\
	size_t back;\
	type buffer[(nentries)];\
}

#define VECQ_INIT(vec) do {\
	memset((vec), 0, sizeof(*(vec)));\
	(vec)->capacity = (sizeof((vec)->buffer) / sizeof((vec)->buffer[0]));\
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
((vec)->front += 1, (vec)->buffer[VECQ_FRONT_POS(vec) - 1])

#define VECQ_SIZE(vec)\
((vec)->back - (vec)->front)

#define VECQ_ENQUEUE(vec, element) do {\
	ASSERT((vec)->capacity != VECQ_SIZE(vec));\
	VECQ_BACK(vec) = element;\
	(vec)->back += 1;\
} while (0)

#define VECQ_CAPACITY(vec)\
((vec)->capacity)

#define VECQ_CLEAR(vec) do {\
	(vec)->front = 0;\
	(vec)->back = 0;\
} while (0)

#endif /* PMDK_VECQ_H */
