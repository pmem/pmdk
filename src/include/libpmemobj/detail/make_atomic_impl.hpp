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
 * make_persistent_impl.hpp -- implementation details of atomic allocation
 * and construction.
 */

#ifndef LIBPMEMOBJ_MAKE_ATOMIC_IMPL_HPP
#define LIBPMEMOBJ_MAKE_ATOMIC_IMPL_HPP

#include <new>

#include "libpmemobj/detail/integer_sequence.hpp"

namespace nvml {

namespace detail {

	/*
	 * Calls the objects constructor.
	 *
	 * Unpacks the tuple to get constructor's parameters.
	 */
	template<typename T, size_t... Indices, typename... Args>
	void create_object(void *ptr, index_sequence<Indices...>,
			std::tuple<Args...> &tuple)
	{
		new (ptr) T(std::get<Indices>(tuple)...);
	}

	/*
	 * C-style function called by the allocator.
	 *
	 * The arg is a tuple containing constructor parameters.
	 */
	template<typename T, typename... Args>
	void obj_constructor(PMEMobjpool *pop, void *ptr, void *arg)
	{
		auto *arg_pack = static_cast<std::tuple<Args...> *>(arg);

		typedef typename make_index_sequence<Args...>::type index;
		create_object<T>(ptr, index(), *arg_pack);

		pmemobj_persist(pop, ptr, sizeof(T));
	}

}  /* namespace detail */

}  /* namespace nvml */

#endif /* LIBPMEMOBJ_MAKE_ATOMIC_IMPL_HPP */
