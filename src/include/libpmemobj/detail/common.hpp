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

/**
 * @file
 * Commonly used functionality.
 */

#ifndef PMEMOBJ_COMMON_HPP
#define PMEMOBJ_COMMON_HPP

#include "libpmemobj.h"
#include "libpmemobj/detail/pexceptions.hpp"
#include <typeinfo>

namespace nvml
{

namespace detail
{

/*
 * Conditionally add an object to a transaction.
 *
 * Adds `*that` to the transaction if it is within a pmemobj pool and
 * there is an active transaction. Does nothing otherwise.
 *
 * @param[in] that pointer to the object being added to the transaction.
 */
template <typename T>
inline void
conditional_add_to_tx(const T *that)
{
	/* 'that' is not in any open pool */
	if (!pmemobj_pool_by_ptr(that))
		return;

	if (pmemobj_tx_stage() != TX_STAGE_WORK)
		return;

	if (pmemobj_tx_add_range_direct(that, sizeof(*that)))
		throw transaction_error("Could not add an object to the"
					" transaction.");
}

/*
 * Return type number for given type.
 */
template <typename T>
constexpr uint64_t
type_num()
{
	return typeid(T).hash_code();
}

} /* namespace detail */

} /* namespace nvml */

#endif /* PMEMOBJ_COMMON_HPP */
