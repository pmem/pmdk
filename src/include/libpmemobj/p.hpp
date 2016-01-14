/*
 * Copyright (c) 2015-2016, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * p.hpp -- resides on pmem property template
 */

#ifndef P_HPP
#define P_HPP

#include <memory>
#include "libpmemobj.h"

#include "libpmemobj/detail/specialization.hpp"

namespace nvml
{

namespace obj
{
	/*
	 * Resides on pmem class.
	 *
	 * p class is a property-like template class that has to be used for all
	 * variables (excluding persistent pointers) which are used in a pmemobj
	 * transactions. The only thing it does is creating snapshots of the
	 * object when modified in the transaction scope.
	 */
	template<typename T>
	class p
	{
		typedef p<T> this_type;
	public:
		p(const T &_val) noexcept : val{_val}
		{
		}

		p() = default;

		p& operator=(const p &rhs) noexcept
		{
			if (pmemobj_tx_stage() == TX_STAGE_WORK)
				pmemobj_tx_add_range_direct(this, sizeof(T));

			this_type(rhs).swap(*this);

			return *this;
		}

		operator T() const noexcept
		{
			return val;
		}

		/*
		 * Swaps two p objects of the same type.
		 */
		void swap(p &other) noexcept
		{
			std::swap(val, other.val);
		}
	private:
		T val;
	};

	/*
	 * Swaps two p objects of the same type.
	 *
	 * Non-member swap function as required by Swappable concept.
	 * en.cppreference.com/w/cpp/concept/Swappable
	 */
	template<class T> inline void
	swap(p<T> & a, p<T> & b) noexcept
	{
		a.swap(b);
	}

} /* namespace obj */

} /* namespace nvml */

#endif /* P_HPP */
