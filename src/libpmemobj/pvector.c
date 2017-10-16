/*
 * Copyright 2016-2017, Intel Corporation
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

	size_t iter; /* a simple embedded iterator value. */
};

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
	ctx->pop = pop;
	ctx->vec = vec;
	ctx->iter = 0;

	/*
	 * First the arrays are traversed to find position of the last element.
	 * To save some time calculating the sum of the sequence at each step
	 * the number of values from the array is added to the global nvalues
	 * counter.
	 */
	size_t narrays;
	for (narrays = 0; narrays < PVECTOR_MAX_ARRAYS; ++narrays) {
		if (vec->arrays[narrays] == 0)
			break;

		if (narrays != PVECTOR_MAX_ARRAYS - 1 &&
			vec->arrays[narrays + 1] != 0)
			ctx->nvalues += 1ULL << (narrays + PVECTOR_INIT_SHIFT);
	}

	if (narrays != 0) {
		/*
		 * But if the last array is found and non-null it needs to be
		 * iterated over to count the number of values present.
		 */
		size_t last_array = narrays - 1;

		size_t arr_size = 1ULL << (last_array + PVECTOR_INIT_SHIFT);
		uint64_t *arrp = OBJ_OFF_TO_PTR(pop, vec->arrays[last_array]);
		size_t nvalues;
		for (nvalues = 0; nvalues < arr_size; ++nvalues) {
			if (arrp[nvalues] == 0)
				break;
		}

		/*
		 * If the last array is not the embedded one and is empty
		 * it means that the application was interrupted in either
		 * the push_back or pop_back methods. Either way there's really
		 * no point in keeping the array.
		 */
		if (nvalues == 0 && last_array != 0 /* 0 array is embedded*/)
			pfree(pop, &vec->arrays[last_array]);
		else
			ctx->nvalues += nvalues;
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
		size_t arr_size = 1ULL << (n + PVECTOR_INIT_SHIFT);
		uint64_t *arrp = OBJ_OFF_TO_PTR(ctx->pop, ctx->vec->arrays[n]);
		VALGRIND_ANNOTATE_NEW_MEMORY(arrp, sizeof(*arrp) * arr_size);
	}
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
 * pvector_push_back -- bumps the number of values in the vector and returns
 *	the pointer to the value position to which the caller must set the
 *	value. Calling this method without actually setting the value will
 *	result in an inconsistent vector state.
 */
uint64_t *
pvector_push_back(struct pvector_context *ctx)
{
	uint64_t idx = ctx->nvalues;
	struct array_spec s = pvector_get_array_spec(idx);
	if (s.idx >= PVECTOR_MAX_ARRAYS) {
		ERR("Exceeded maximum number of entries in persistent vector");
		return NULL;
	}
	PMEMobjpool *pop = ctx->pop;

	/*
	 * If the destination array does not exist, calculate its size
	 * and allocate it.
	 */
	if (ctx->vec->arrays[s.idx] == 0) {
		if (s.idx == 0) {
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
				(1ULL << (s.idx + PVECTOR_INIT_SHIFT));

			if (pmalloc_construct(pop,
				&ctx->vec->arrays[s.idx],
				arr_size, pvector_array_constr, NULL,
				0, OBJ_INTERNAL_OBJECT_MASK, 0) != 0)
					return NULL;
		}
	}

	ctx->nvalues++;
	uint64_t *arrp = OBJ_OFF_TO_PTR(pop, ctx->vec->arrays[s.idx]);

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

	if (s.pos == 0 && s.idx != 0 /* the array 0 is embedded */) {
#ifdef USE_VG_PMEMCHECK
		if (On_valgrind) {
			size_t usable_size = palloc_usable_size(&ctx->pop->heap,
				ctx->vec->arrays[s.idx]);
			VALGRIND_REMOVE_FROM_TX(arrp, usable_size);
		}
#endif
		pfree(ctx->pop, &ctx->vec->arrays[s.idx]);
	}

	ctx->nvalues--;

	return ret;
}

/*
 * pvector_nvalues -- returns the number of values present in the vector
 */
uint64_t
pvector_nvalues(struct pvector_context *ctx)
{
	return ctx->nvalues;
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
