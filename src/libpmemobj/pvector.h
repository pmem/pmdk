/*
 * Copyright 2016, Intel Corporation
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
 * pvector.h -- internal definitions for persistent vector
 *
 * The structure defined here is a non-reallocating vector whose values
 * are stored in a array of arrays, where the sizes of each consecutive array
 * form a geometric sequence with common ratio of 2.
 */

/*
 * The PVECTOR_INIT_SHIFT and PVECTOR_INIT_SIZE sets the initial
 * size of the vector. In other words, the shift defines from which term
 * in the geometric sequence does the calculations start and the size is the
 * calculated term value.
 */
#define PVECTOR_INIT_SHIFT (3ULL)
#define PVECTOR_INIT_SIZE (1ULL << PVECTOR_INIT_SHIFT)

/*
 * The maximum number of arrays that can be allocated in a single vector.
 * This sets the hard limit of number of values in the vector - which is the
 * sum of the geometric sequence.
 */
#define PVECTOR_MAX_ARRAYS (20)

struct pvector_context;

struct pvector {
	uint64_t arrays[PVECTOR_MAX_ARRAYS]; /* offset to the array object */

	/*
	 * Because the assumption is that most of the vector uses won't exceed
	 * a relatively small number of entries, the first array is embedded
	 * directly into the structure.
	 */
	uint64_t embedded[PVECTOR_INIT_SIZE];
};

typedef void (*entry_op_callback)(PMEMobjpool *pop, uint64_t *entry);

struct pvector_context *pvector_init(PMEMobjpool *pop, struct pvector *vec);
void pvector_delete(struct pvector_context *ctx);

uint64_t *pvector_push_back(struct pvector_context *ctx);

uint64_t pvector_pop_back(struct pvector_context *ctx,
	entry_op_callback cb);

uint64_t pvector_nvalues(struct pvector_context *ctx);
uint64_t pvector_first(struct pvector_context *ctx);
uint64_t pvector_last(struct pvector_context *ctx);
uint64_t pvector_prev(struct pvector_context *ctx);
uint64_t pvector_next(struct pvector_context *ctx);
