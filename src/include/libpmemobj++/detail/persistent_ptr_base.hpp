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

#ifndef PMEMOBJ_PERSISTENT_PTR_BASE_HPP
#define PMEMOBJ_PERSISTENT_PTR_BASE_HPP

#include <type_traits>

#include "libpmemobj++/detail/common.hpp"
#include "libpmemobj++/detail/specialization.hpp"
#include "libpmemobj.h"

/* Windows has a max macro which collides with std::numeric_limits::max */
#if defined(max) && defined(_WIN32)
#undef max
#endif

namespace nvml
{

namespace detail
{

/**
 * Persistent_ptr base class
 *
 * Implements some of the functionality of the persistent_ptr class. It defines
 * all applicable conversions from and to a persistent_ptr_base. This class is
 * an implementation detail and is not to be instantiated. It cannot be declared
 * as virtual due to the problem with rebuilding the vtable.
 */
template <typename T>
class persistent_ptr_base {
	typedef persistent_ptr_base<T> this_type;

	template <typename U>
	friend class persistent_ptr_base;

public:
	/**
	 * Type of an actual object with all qualifier removed,
	 * used for easy underlying type access
	 */
	typedef typename nvml::detail::sp_element<T>::type element_type;

	/**
	 * Default constructor, zeroes the PMEMoid.
	 */
	persistent_ptr_base() : oid(OID_NULL)
	{
		verify_type();
	}

	/*
	 * Curly braces initialization is not used because the
	 * PMEMoid is a plain C (POD) type and we can't add a default
	 * constructor in there.
	 */

	/**
	 * PMEMoid constructor.
	 *
	 * Provided for easy interoperability between C++ and C API's.
	 *
	 * @param oid C-style persistent pointer
	 */
	persistent_ptr_base(PMEMoid oid) noexcept : oid(oid)
	{
		verify_type();
	}

	/**
	 * Volatile pointer constructor.
	 *
	 * If ptr does not point to an address from a valid pool, the persistent
	 * pointer will evaluate to nullptr.
	 *
	 * @param ptr volatile pointer, pointing to persistent memory.
	 */
	persistent_ptr_base(element_type *ptr) : oid(pmemobj_oid(ptr))
	{
		verify_type();
	}

	/**
	 * Copy constructor from a different persistent_ptr<>.
	 *
	 * Available only for convertible types.
	 */
	template <typename U,
		  typename = typename std::enable_if<
			  !std::is_same<T, U>::value &&
			  std::is_same<typename std::remove_cv<T>::type,
				       U>::value>::type>
	persistent_ptr_base(persistent_ptr_base<U> const &r) noexcept
		: oid(r.oid)
	{
		this->oid.off += calculate_offset<U>();
		verify_type();
	}

	/**
	 * Copy constructor from a different persistent_ptr<>.
	 *
	 * Available only for convertible, non-void types.
	 */
	template <
		typename U, typename Dummy = void,
		typename = typename std::enable_if<
			!std::is_same<
				typename std::remove_cv<T>::type,
				typename std::remove_cv<U>::type>::value &&
				!std::is_void<U>::value,
			decltype(static_cast<T *>(std::declval<U *>()))>::type>
	persistent_ptr_base(persistent_ptr_base<U> const &r) noexcept
		: oid(r.oid)
	{
		this->oid.off += calculate_offset<U>();
		verify_type();
	}

	/**
	 * Conversion operator to a different persistent_ptr<>.
	 *
	 * Available only for convertible, non-void types.
	 */
	template <
		typename Y,
		typename = typename std::enable_if<
			!std::is_same<
				typename std::remove_cv<T>::type,
				typename std::remove_cv<Y>::type>::value &&
				!std::is_void<Y>::value,
			decltype(static_cast<T *>(std::declval<Y *>()))>::type>
	operator persistent_ptr_base<Y>() noexcept
	{
		/*
		 * The offset conversion should not be required here.
		 */
		return persistent_ptr_base<Y>(this->oid);
	}

	/*
	 * Copy constructor.
	 *
	 * @param r Persistent pointer to the same type.
	 */
	persistent_ptr_base(persistent_ptr_base const &r) noexcept : oid(r.oid)
	{
		verify_type();
	}

	/**
	 * Defaulted move constructor.
	 */
	persistent_ptr_base(persistent_ptr_base &&r) noexcept
		: oid(std::move(r.oid))
	{
		verify_type();
	}

	/**
	 * Defaulted move assignment operator.
	 */
	persistent_ptr_base &
	operator=(persistent_ptr_base &&r)
	{
		detail::conditional_add_to_tx(this);
		this->oid = std::move(r.oid);

		return *this;
	}

	/**
	 * Assignment operator.
	 *
	 * Persistent pointer assignment within a transaction
	 * automatically registers this operation so that a rollback
	 * is possible.
	 *
	 * @throw nvml::transaction_error when adding the object to the
	 *	transaction failed.
	 */
	persistent_ptr_base &
	operator=(persistent_ptr_base const &r)
	{
		this_type(r).swap(*this);

		return *this;
	}

	/**
	 * Nullptr move assignment operator.
	 *
	 * @throw nvml::transaction_error when adding the object to the
	 *	transaction failed.
	 */
	persistent_ptr_base &
	operator=(std::nullptr_t &&)
	{
		detail::conditional_add_to_tx(this);
		this->oid = {0, 0};
		return *this;
	}

	/**
	 * Converting assignment operator from a different
	 * persistent_ptr<>.
	 *
	 * Available only for convertible types.
	 * Just like regular assignment, also automatically registers
	 * itself in a transaction.
	 *
	 * @throw nvml::transaction_error when adding the object to the
	 *	transaction failed.
	 */
	template <typename Y,
		  typename = typename std::enable_if<
			  std::is_convertible<Y *, T *>::value>::type>
	persistent_ptr_base &
	operator=(persistent_ptr_base<Y> const &r)
	{
		this_type(r).swap(*this);

		return *this;
	}

	/**
	 * Swaps two persistent_ptr objects of the same type.
	 *
	 * @param[in,out] other the other persistent_ptr to swap.
	 */
	void
	swap(persistent_ptr_base &other)
	{
		detail::conditional_add_to_tx(this);
		detail::conditional_add_to_tx(&other);
		std::swap(this->oid, other.oid);
	}

	/**
	 * Get a direct pointer.
	 *
	 * Performs a calculations on the underlying C-style pointer.
	 *
	 * @return a direct pointer to the object.
	 */
	element_type *
	get() const noexcept
	{
		if (this->oid.pool_uuid_lo ==
		    std::numeric_limits<decltype(oid.pool_uuid_lo)>::max())
			return reinterpret_cast<element_type *>(oid.off);
		else
			return static_cast<element_type *>(
				pmemobj_direct(this->oid));
	}

	/**
	 * Get PMEMoid encapsulated by this object.
	 *
	 * For C API compatibility.
	 *
	 * @return const reference to the PMEMoid
	 */
	const PMEMoid &
	raw() const noexcept
	{
		return this->oid;
	}

	/**
	 * Get pointer to PMEMoid encapsulated by this object.
	 *
	 * For C API compatibility.
	 *
	 * @return pointer to the PMEMoid
	 */
	PMEMoid *
	raw_ptr() noexcept
	{
		return &(this->oid);
	}

	/*
	 * Bool conversion operator.
	 */
	explicit operator bool() const noexcept
	{
		return get() != nullptr;
	}

protected:
	/* The underlying PMEMoid of the held object. */
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
		static_assert(!std::is_polymorphic<element_type>::value,
			      "Polymorphic types are not supported");
	}

	/**
	 * Private constructor enabling persistent_ptrs to volatile objects.
	 *
	 * This is internal implementation only needed for the
	 * pointer_traits<persistent_ptr>::pointer_to to be able to create
	 * valid pointers. This is used in libstdc++'s std::vector::insert().
	 */
	persistent_ptr_base(element_type *vptr, int) : persistent_ptr_base(vptr)
	{
		if (OID_IS_NULL(oid)) {
			oid.pool_uuid_lo = std::numeric_limits<decltype(
				oid.pool_uuid_lo)>::max();
			oid.off = reinterpret_cast<decltype(oid.off)>(vptr);
		}
	}

	/**
	 * Calculate in-object offset for structures with inheritance.
	 *
	 * In case of the given inheritance:
	 *
	 *	  A   B
	 *	   \ /
	 *	    C
	 *
	 * A pointer to B *ptr = &C should be offset by sizeof(A). This function
	 * calculates that offset.
	 *
	 * @return offset between to compatible pointer types to the same object
	 */
	template <typename U>
	inline ptrdiff_t
	calculate_offset() const
	{
		static const ptrdiff_t ptr_offset_magic = 0xDEADBEEF;

		U *tmp{reinterpret_cast<U *>(ptr_offset_magic)};
		T *diff = static_cast<T *>(tmp);
		return reinterpret_cast<ptrdiff_t>(diff) -
			reinterpret_cast<ptrdiff_t>(tmp);
	}
};

} /* namespace detail */

} /* namespace nvml */

#endif /* PMEMOBJ_PERSISTENT_PTR_BASE_HPP */
