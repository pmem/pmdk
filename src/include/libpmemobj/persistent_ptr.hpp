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
 * persistent_ptr.hpp -- persistent smart pointer
 */

#ifndef PERSISTENT_PTR_HPP
#define PERSISTENT_PTR_HPP

#include <memory>
#include <assert.h>
#include "libpmemobj.h"

#include "libpmemobj/detail/specialization.hpp"

namespace nvml
{

namespace obj
{
	/*
	 * Persistent pointer class.
	 *
	 * persistent_ptr implements a smart ptr. It encapsulates the PMEMoid
	 * fat pointer and provides member access, dereference and array
	 * access operators.
	 * This type does NOT manage the life-cycle of the object.
	 */
	template<typename T>
	class persistent_ptr
	{
		template<typename Y>
		friend class persistent_ptr;

		typedef persistent_ptr<T> this_type;
	public:
		/* used for easy underlying type access */
		typedef typename nvml::detail::sp_element<T>::type element_type;

		persistent_ptr() = default;

		/*
		 * Curly braces initialization is not used because the
		 * PMEMoid is a plain C (POD) type and we can't add a default
		 * constructor in there.
		 */

		/* default null constructor, zeroes the PMEMoid */
		persistent_ptr(std::nullptr_t) noexcept : oid(OID_NULL)
		{
			verify_type();
		}

		persistent_ptr(PMEMoid o) noexcept : oid(o)
		{
			verify_type();
		}

		template<typename Y>
		persistent_ptr(const persistent_ptr<Y> &r) noexcept : oid(r.oid)
		{
			verify_type();
			static_assert(std::is_convertible<Y, T>::value,
				"constructor from inconvertible type");
		}

		persistent_ptr(const persistent_ptr &r) noexcept : oid(r.oid)
		{
			verify_type();
		}

		persistent_ptr(persistent_ptr &&r) noexcept = default;

		persistent_ptr &
		operator=(persistent_ptr &&r) noexcept = default;

		persistent_ptr &
		operator=(const persistent_ptr &r) noexcept
		{
			if (pmemobj_tx_stage() == TX_STAGE_WORK) {
				pmemobj_tx_add_range_direct(this,
					sizeof (*this));
			}

			this_type(r).swap(*this);

			return *this;
		}

		template<typename Y>
		persistent_ptr &
		operator=(const persistent_ptr<Y> &r) noexcept
		{
			static_assert(std::is_convertible<Y, T>::value,
				"assignment of inconvertible types");

			if (pmemobj_tx_stage() == TX_STAGE_WORK) {
				pmemobj_tx_add_range_direct(this,
					sizeof (*this));
			}

			this_type(r).swap(*this);

			return *this;
		}

		typename nvml::detail::sp_dereference<T>::type
			operator*() const noexcept
		{
			return *get();
		}

		typename nvml::detail::sp_member_access<T>::type
			operator->() const noexcept
		{
			return get();
		}

		typename nvml::detail::sp_array_access<T>::type
			operator[](std::ptrdiff_t i) const noexcept
		{
			assert(i >= 0 &&
				(i < nvml::detail::sp_extent<T>::value ||
				nvml::detail::sp_extent<T>::value == 0) &&
				"persistent array index out of bounds");

			return get()[i];
		}

		/*
		 * Get a direct pointer.
		 *
		 * Calculates and returns a direct pointer to the object.
		 */
		element_type *
		get() const noexcept
		{
			return (element_type *)pmemobj_direct(oid);
		}

		/*
		 * Swaps two persistent_ptr objects of the same type.
		 */
		void
		swap(persistent_ptr &other) noexcept
		{
			std::swap(oid, other.oid);
		}

		/* unspecified bool type */
		typedef element_type *
			(persistent_ptr<T>::*unspecified_bool_type)() const;

		operator unspecified_bool_type() const noexcept
		{
			return OID_IS_NULL(oid) ? 0 : &persistent_ptr<T>::get;
		}

		explicit operator bool() const noexcept
		{
			return get() != nullptr;
		}

		/*
		 * Get PMEMoid encapsulated by this object.
		 *
		 * For C API compatibility.
		 */
		const PMEMoid &
		raw() const noexcept
		{
			return oid;
		}

		/*
		 * Get pointer to PMEMoid encapsulated by this object.
		 *
		 * For C API compatibility.
		 */
		PMEMoid *
		raw_ptr() noexcept
		{
			return &oid;
		}
	private:
		PMEMoid oid;

		/*
		 * C++ persistent memory support has following type limitations:
		 * en.cppreference.com/w/cpp/types/is_polymorphic
		 * en.cppreference.com/w/cpp/types/is_default_constructible
		 * en.cppreference.com/w/cpp/types/is_destructible
		 */
		void
		verify_type()
		{
			static_assert(!std::is_polymorphic
				<element_type>::value,
			"Polymorphic types are not supported");
		}
	};

	/*
	 * Swaps two persistent_ptr objects of the same type.
	 *
	 * Non-member swap function as required by Swappable concept.
	 * en.cppreference.com/w/cpp/concept/Swappable
	 */
	template<class T> inline void
	swap(persistent_ptr<T> & a, persistent_ptr<T> & b) noexcept
	{
		a.swap(b);
	}

} /* namespace obj */

} /* namespace nvml */

#endif /* PERSISTENT_PTR_HPP */
