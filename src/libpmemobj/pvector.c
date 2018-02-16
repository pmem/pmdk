/*
 * Copyright 2016-2018, Intel Corporation
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
 * pvector.c -- persistent vector implementation
 */

#include "obj.h"
#include "out.h"
#include "pmalloc.h"
#include "pvector.h"
#include "valgrind_internal.h"

struct pvector_context {
	PMEMobjpool *pop;
	struct pvector *vec;
	size_t nvalues;
	size_t capacity;

	size_t iter; /* a simple embedded iterator value. */
};

/*
 * pvector_arr_size -- (internal) returns the number of entries of an array with
 *	the given index
 */
static size_t
pvector_arr_size(size_t idx)
{
	return 1ULL << (idx + PVECTOR_INIT_SHIFT);
}

/*
 * pvector_new -- allocates and initializes persistent vector runtime context.
 *
 * To make sure the runtime information is correct (the number of values) the
 * persistent vector is iterated through and appropriate metrics are measured.
 */
struct pvector_context *
pvector_new(PMEMobjpool *pop, struct pvector *vec)
{
	struct pvector_context *ctx = Malloc(sizeof(*ctx));
	if (ctx == NULL) {
		ERR("!failed to create pvector context");
		return NULL;
	}
	ctx->nvalues = 0;
	ctx->capacity = 0;
	ctx->pop = pop;
	ctx->vec = vec;
	ctx->iter = 0;

	/*
	 * Traverse the entire vector backwards, if an array is entirely zeroed,
	 * free it, otherwise count all of its elements.
	 */
	for (int i = PVECTOR_MAX_ARRAYS - 1; i >= 0; --i) {
		if (vec->arrays[i] == 0)
			continue;

		uint64_t *arrp = OBJ_OFF_TO_PTR(pop, vec->arrays[i]);

		size_t arr_size = pvector_arr_size((size_t)i);
		size_t nvalues = arr_size;
		/* only count nvalues for the last array */
		if (i == PVECTOR_MAX_ARRAYS - 1 || vec->arrays[i + 1] == 0) {
			nvalues = 0;
			size_t nzeros = 0;
			/* zero entries are valid, so count them too */
			for (size_t n = 0; n < arr_size; ++n) {
				if (arrp[n] != 0) {
					nvalues += nzeros + 1;
					nzeros = 0;
				} else {
					nzeros++;
				}
			}
		}

		if (nvalues == 0 && i != 0 /* skip embedded array */) {
			pfree(pop, &vec->arrays[i]);
		} else {
			ctx->nvalues += nvalues;
			ctx->capacity += arr_size;
		}
	}

	return ctx;
}

/*
 * pvector_delete -- deletes the runtime state of the vector. Has no impact
 *	on the persistent representation of the vector.
 */
void
pvector_delete(struct pvector_context *ctx)
{
	Free(ctx);
}

/*
 * pvector_reinit -- reinitializes the pvector runtime data
 */
void
pvector_reinit(struct pvector_context *ctx)
{
	VALGRIND_ANNOTATE_NEW_MEMORY(ctx, sizeof(*ctx));
	for (size_t n = 1; n < PVECTOR_MAX_ARRAYS; ++n) {
		if (ctx->vec->arrays[n] == 0)
			break;
		size_t arr_size = pvector_arr_size(n);
		uint64_t *arrp = OBJ_OFF_TO_PTR(ctx->pop, ctx->vec->arrays[n]);
		VALGRIND_ANNOTATE_NEW_MEMORY(arrp, sizeof(*arrp) * arr_size);
	}
}

/*
 * pvector_size -- returns the number of elements in the vector
 */
size_t
pvector_size(struct pvector_context *ctx)
{
	return ctx->nvalues;
}

/*
 * pvector_capacity -- returns the size of allocated memory capacity
 */
size_t
pvector_capacity(struct pvector_context *ctx)
{
#ifdef DEBUG
	size_t capacity = 0;
	for (size_t i = 0; i < PVECTOR_MAX_ARRAYS; ++i) {
		if (ctx->vec->arrays[i] == 0)
			break;
		capacity += pvector_arr_size(i);
	}
	ASSERTeq(ctx->capacity, capacity);
#endif
	return ctx->capacity;
}

/*
 * A small helper structure that defines the position of a value in the array
 * of arrays.
 */
struct array_spec {
	size_t idx; /* The index of array in sequence */
	size_t pos; /* The position in the array */
};

/*
 * pvector_get_array_spec -- (internal) translates a global vector index
 *	into a more concrete position in the array.
 */
static struct array_spec
pvector_get_array_spec(uint64_t idx)
{
	struct array_spec s;

	/*
	 * Search for the correct array by looking at the highest bit of the
	 * element position (offset by the size of initial array), which
	 * represents its capacity and position in the array of arrays.
	 *
	 * Because the vector has large initial embedded array the position bit
	 * that was calculated must take that into consideration and subtract
	 * the bit position from which the algorithm starts.
	 */
	uint64_t pos = idx + PVECTOR_INIT_SIZE;
	unsigned hbit = util_mssb_index64(pos);
	s.idx = (size_t)(hbit - PVECTOR_INIT_SHIFT);

	/*
	 * To find the actual position of the element in the array we simply
	 * mask the bits of the position that correspond to the size of
	 * the array. In other words this is: pos - 2^[array index].
	 */
	s.pos = pos ^ (1ULL << hbit);

	return s;
}

/*
 * pvector_array_constr -- (internal) constructor of a new vector array.
 *
 * The vectors MUST be zeroed because non-zero array elements are treated as
 * vector values.
 */
static int
pvector_array_constr(void *ctx, void *ptr, size_t usable_size, void *arg)
{
	PMEMobjpool *pop = ctx;

	/*
	 * Vectors are used as transaction logs, valgrind shouldn't warn about
	 * storing things inside of them.
	 * This memory range is removed from tx when the array is freed as a
	 * result of pop_back or when the transaction itself ends.
	 */
	VALGRIND_ADD_TO_TX(ptr, usable_size);

	pmemops_memset_persist(&pop->p_ops, ptr, 0, usable_size);

	return 0;
}

/*
 * pvector_reserve -- attempts to reserve memory for at least size entries
 */
int
pvector_reserve(struct pvector_context *ctx, size_t size)
{
	if (size <= pvector_capacity(ctx))
		return 0;

	struct array_spec s = pvector_get_array_spec(size);
	if (s.idx >= PVECTOR_MAX_ARRAYS) {
		ERR("Exceeded maximum number of entries in persistent vector");
		return -1;
	}
	PMEMobjpool *pop = ctx->pop;

	for (int i = (int)s.idx; i >= 0; --i) {
		if (ctx->vec->arrays[i] != 0)
			continue;

		if (i == 0) {
			/*
			 * In the case the vector is completely empty the
			 * initial embedded array must be assigned as the first
			 * element of the sequence.
			 */
			ASSERTeq(util_is_zeroed(ctx->vec,
				sizeof(*ctx->vec)), 1);

			ctx->vec->arrays[0] = OBJ_PTR_TO_OFF(pop,
				&ctx->vec->embedded);

			pmemops_persist(&pop->p_ops, &ctx->vec->arrays[0],
				sizeof(ctx->vec->arrays[0]));
		} else {
			size_t arr_size = sizeof(uint64_t) *
				pvector_arr_size((size_t)i);

			if (pmalloc_construct(pop,
				&ctx->vec->arrays[i],
				arr_size, pvector_array_constr, NULL,
				0, OBJ_INTERNAL_OBJECT_MASK, 0) != 0)
					return -1;
		}
		ctx->capacity += pvector_arr_size((size_t)i);
	}

	return 0;
}

/*
 * pvector_push_back -- bumps the number of values in the vector and returns
 *	the pointer to the value position to which the caller must set the
 *	value. Calling this method without actually setting the value will
 *	result in an inconsistent vector state.
 */
uint64_t *
pvector_push_back(struct pvector_context *ctx)
{
	if (pvector_reserve(ctx, ctx->nvalues + 1) != 0)
		return NULL;

	uint64_t idx = ctx->nvalues;

	struct array_spec s = pvector_get_array_spec(idx);
	ASSERTne(ctx->vec->arrays[s.idx], 0);
	uint64_t *arrp = OBJ_OFF_TO_PTR(ctx->pop, ctx->vec->arrays[s.idx]);

	ctx->nvalues++;

	return &arrp[s.pos];
}

/*
 * pvector_pop_back -- decreases the number of values and executes
 *	a user-defined callback in which the caller must zero the value.
 */
uint64_t
pvector_pop_back(struct pvector_context *ctx, entry_op_callback cb)
{
	if (ctx->nvalues == 0)
		return 0;

	uint64_t idx = ctx->nvalues - 1;
	struct array_spec s = pvector_get_array_spec(idx);

	uint64_t *arrp = OBJ_OFF_TO_PTR(ctx->pop, ctx->vec->arrays[s.idx]);
	uint64_t ret = arrp[s.pos];

	if (cb)
		cb(ctx->pop, &arrp[s.pos]);

	ctx->nvalues--;
	if (s.pos != 0)
		return ret;

	/* free all potentially reserved but unused arrays */
	for (int i = PVECTOR_MAX_ARRAYS - 1; i >= (int)s.idx; --i) {
		if (i == 0 || ctx->vec->arrays[i] == 0)
			continue;

#if VG_PMEMCHECK_ENABLED
		if (On_valgrind) {
			size_t usable_size = palloc_usable_size(
				&ctx->pop->heap,
				ctx->vec->arrays[i]);
			VALGRIND_REMOVE_FROM_TX(arrp, usable_size);
		}
#endif
		ctx->capacity -= pvector_arr_size((size_t)i);
		pfree(ctx->pop, &ctx->vec->arrays[i]);
	}

	return ret;
}

/*
 * pvector_get -- returns the vector value at the index.
 */
static uint64_t
pvector_get(PMEMobjpool *pop, struct pvector *vec, uint64_t idx)
{
	struct array_spec s = pvector_get_array_spec(idx);
	uint64_t *arrp = OBJ_OFF_TO_PTR(pop, vec->arrays[s.idx]);

	return arrp[s.pos];
}

/*
 * pvector_first -- sets the iterator position to the first element and returns
 *	the value present at that index.
 */
uint64_t
pvector_first(struct pvector_context *ctx)
{
	if (ctx->nvalues == 0)
		return 0;

	ctx->iter = 0;

	return pvector_get(ctx->pop, ctx->vec, ctx->iter);
}

/*
 * pvector_last -- sets the iterator position to the last element and returns
 *	the value present at that index.
 */
uint64_t
pvector_last(struct pvector_context *ctx)
{
	if (ctx->nvalues == 0)
		return 0;

	ctx->iter = ctx->nvalues - 1;

	return pvector_get(ctx->pop, ctx->vec, ctx->iter);
}

/*
 * pvector_prev -- decreases the iterator index and returns the value. When the
 *	iterator has reached the minimum index returns zero.
 */
uint64_t
pvector_prev(struct pvector_context *ctx)
{
	if (ctx->iter == 0)
		return 0;

	ctx->iter--;

	return pvector_get(ctx->pop, ctx->vec, ctx->iter);
}

/*
 * pvector_next -- increases the iterator index and returns the value.When the
 *	iterator has reached the maximum index returns zero.
 */
uint64_t
pvector_next(struct pvector_context *ctx)
{
	if (ctx->iter == ctx->nvalues - 1)
		return 0;

	ctx->iter++;

	return pvector_get(ctx->pop, ctx->vec, ctx->iter);
}
