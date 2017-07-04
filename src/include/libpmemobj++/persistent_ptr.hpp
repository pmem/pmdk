/*
 * Copyright 2015-2017, Intel Corporation
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
 * Persistent smart pointer.
 */

#ifndef PMEMOBJ_PERSISTENT_PTR_HPP
#define PMEMOBJ_PERSISTENT_PTR_HPP

#include <cassert>
#include <limits>
#include <memory>
#include <ostream>

#include "libpmemobj++/detail/common.hpp"
#include "libpmemobj++/detail/persistent_ptr_base.hpp"
#include "libpmemobj++/detail/specialization.hpp"
#include "libpmemobj++/pool.hpp"
#include "libpmemobj.h"

namespace nvml
{

namespace obj
{

template <typename T>
class pool;

template <typename T>
class persistent_ptr;

/*
 * persistent_ptr void specialization.
 */
template <>
class persistent_ptr<void> : public detail::persistent_ptr_base<void> {
public:
	persistent_ptr() = default;
	using detail::persistent_ptr_base<void>::persistent_ptr_base;
	using detail::persistent_ptr_base<void>::operator=;
};

/*
 * persistent_ptr const void specialization.
 */
template <>
class persistent_ptr<const void>
	: public detail::persistent_ptr_base<const void> {
public:
	persistent_ptr() = default;
	using detail::persistent_ptr_base<const void>::persistent_ptr_base;
	using detail::persistent_ptr_base<const void>::operator=;
};

/**
 * Persistent pointer class.
 *
 * persistent_ptr implements a smart ptr. It encapsulates the PMEMoid
 * fat pointer and provides member access, dereference and array
 * access operators. The persistent_ptr is not designed to work with polymorphic
 * types, as they have runtime RTTI info embedded, which is implementation
 * specific and thus not consistently rebuildable. Such constructs as
 * polymorphic members or members of a union defined within a class held in
 * a persistent_ptr will also yield undefined behavior.
 * This type does NOT manage the life-cycle of the object. The typical usage
 * example would be:
 * @snippet doc_snippets/persistent.cpp persistent_ptr_example
 */
template <typename T>
class persistent_ptr : public detail::persistent_ptr_base<T> {
public:
	persistent_ptr() = default;
	using detail::persistent_ptr_base<T>::persistent_ptr_base;

	/**
	 * Explicit void specialization of the converting constructor.
	 */
	explicit persistent_ptr(persistent_ptr<void> const &rhs) noexcept
		: detail::persistent_ptr_base<T>(rhs.raw())
	{
	}

	/**
	 * Explicit const void specialization of the converting constructor.
	 */
	explicit persistent_ptr(persistent_ptr<const void> const &rhs) noexcept
		: detail::persistent_ptr_base<T>(rhs.raw())
	{
	}

	/**
	 * Persistent pointer to void conversion operator.
	 */
	operator persistent_ptr<void>() const noexcept
	{
		return this->get();
	}

	/**
	 * Dereference operator.
	 */
	typename nvml::detail::sp_dereference<T>::type operator*() const
		noexcept
	{
		return *this->get();
	}

	/**
	 * Member access operator.
	 */
	typename nvml::detail::sp_member_access<T>::type operator->() const
		noexcept
	{
		return this->get();
	}

	/**
	 * Array access operator.
	 *
	 * Contains run-time bounds checking for static arrays.
	 */
	template <typename = typename std::enable_if<!std::is_void<T>::value>>
	typename nvml::detail::sp_array_access<T>::type
	operator[](std::ptrdiff_t i) const noexcept
	{
		assert(i >= 0 && (i < nvml::detail::sp_extent<T>::value ||
				  nvml::detail::sp_extent<T>::value == 0) &&
		       "persistent array index out of bounds");

		return this->get()[i];
	}

	/**
	 * Prefix increment operator.
	 */
	inline persistent_ptr<T> &operator++()
	{
		detail::conditional_add_to_tx(this);
		this->oid.off += sizeof(T);

		return *this;
	}

	/**
	 * Postfix increment operator.
	 */
	inline persistent_ptr<T> operator++(int)
	{
		PMEMoid noid = this->oid;
		++(*this);

		return persistent_ptr<T>(noid);
	}

	/**
	 * Prefix decrement operator.
	 */
	inline persistent_ptr<T> &operator--()
	{
		detail::conditional_add_to_tx(this);
		this->oid.off -= sizeof(T);

		return *this;
	}

	/**
	 * Postfix decrement operator.
	 */
	inline persistent_ptr<T> operator--(int)
	{
		PMEMoid noid = this->oid;
		--(*this);

		return persistent_ptr<T>(noid);
	}

	/**
	 * Addition assignment operator.
	 */
	inline persistent_ptr<T> &
	operator+=(std::ptrdiff_t s)
	{
		detail::conditional_add_to_tx(this);
		this->oid.off += s * sizeof(T);

		return *this;
	}

	/**
	 * Subtraction assignment operator.
	 */
	inline persistent_ptr<T> &
	operator-=(std::ptrdiff_t s)
	{
		detail::conditional_add_to_tx(this);
		this->oid.off -= s * sizeof(T);

		return *this;
	}

	/**
	 * Persists the content of the underlying object.
	 *
	 * @param[in] pop Pmemobj pool
	 */
	void
	persist(pool_base &pop)
	{
		pop.persist(this->get(), sizeof(T));
	}

	/**
	 * Persists what the persistent pointer points to.
	 *
	 * @throw pool_error when cannot get pool from persistent
	 * pointer
	 */
	void
	persist(void)
	{
		pmemobjpool *pop = pmemobj_pool_by_oid(this->raw());

		if (pop == nullptr)
			throw pool_error("Cannot get pool from "
					 "persistent pointer");

		pmemobj_persist(pop, this->get(), sizeof(T));
	}

	/**
	 * Flushes what the persistent pointer points to.
	 *
	 * @param[in] pop Pmemobj pool
	 */
	void
	flush(pool_base &pop)
	{
		pop.flush(this->get(), sizeof(T));
	}

	/**
	 * Flushes what the persistent pointer points to.
	 *
	 * @throw pool_error when cannot get pool from persistent
	 * pointer
	 */
	void
	flush(void)
	{
		pmemobjpool *pop = pmemobj_pool_by_oid(this->raw());

		if (pop == nullptr)
			throw pool_error("Cannot get pool from "
					 "persistent pointer");

		pmemobj_flush(pop, this->get(), sizeof(T));
	}

	/*
	 * Pointer traits related.
	 */

	/**
	 * Create a persistent pointer from a given reference.
	 *
	 * This can create a persistent_ptr to a volatile object, use with
	 * extreme caution.
	 *
	 * @param ref reference to an object.
	 */
	static persistent_ptr<T>
	pointer_to(T &ref)
	{
		return persistent_ptr<T>(std::addressof(ref), 0);
	}

	/**
	 * Rebind to a different type of pointer.
	 */
	template <class U>
	using rebind = nvml::obj::persistent_ptr<U>;

	/**
	 * The persistency type to be used with this pointer.
	 */
	using persistency_type = p<T>;

	/**
	 * The used bool_type.
	 */
	using bool_type = bool;

	/*
	 * Random access iterator requirements (members)
	 */

	/**
	 * The persistent_ptr iterator category.
	 */
	using iterator_category = std::random_access_iterator_tag;

	/**
	 * The persistent_ptr difference type.
	 */
	using difference_type = std::ptrdiff_t;

	/**
	 * The type of the value pointed to by the persistent_ptr.
	 */
	using value_type = T;

	/**
	 * The reference type of the value pointed to by the persistent_ptr.
	 */
	using reference = T &;

	/**
	 * The pointer type.
	 */
	using pointer = persistent_ptr<T>;
};

/**
 * Swaps two persistent_ptr objects of the same type.
 *
 * Non-member swap function as required by Swappable concept.
 * en.cppreference.com/w/cpp/concept/Swappable
 */
template <class T>
inline void
swap(persistent_ptr<T> &a, persistent_ptr<T> &b)
{
	a.swap(b);
}

/**
 * Equality operator.
 *
 * This checks if underlying PMEMoids are equal.
 */
template <typename T, typename Y>
inline bool
operator==(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	return OID_EQUALS(lhs.raw(), rhs.raw());
}

/**
 * Inequality operator.
 */
template <typename T, typename Y>
inline bool
operator!=(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	return !(lhs == rhs);
}

/**
 * Equality operator with nullptr.
 */
template <typename T>
inline bool
operator==(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return lhs.get() == nullptr;
}

/**
 * Equality operator with nullptr.
 */
template <typename T>
inline bool
operator==(std::nullptr_t, persistent_ptr<T> const &lhs) noexcept
{
	return lhs.get() == nullptr;
}

/**
 * Inequality operator with nullptr.
 */
template <typename T>
inline bool
operator!=(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return lhs.get() != nullptr;
}

/**
 * Inequality operator with nullptr.
 */
template <typename T>
inline bool
operator!=(std::nullptr_t, persistent_ptr<T> const &lhs) noexcept
{
	return lhs.get() != nullptr;
}

/**
 * Less than operator.
 *
 * @return true if the uuid_lo of lhs is less than the uuid_lo of rhs,
 * should they be equal, the offsets are compared. Returns false
 * otherwise.
 */
template <typename T, typename Y>
inline bool
operator<(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	if (lhs.raw().pool_uuid_lo == rhs.raw().pool_uuid_lo)
		return lhs.raw().off < rhs.raw().off;
	else
		return lhs.raw().pool_uuid_lo < rhs.raw().pool_uuid_lo;
}

/**
 * Less or equal than operator.
 *
 * See less than operator for comparison rules.
 */
template <typename T, typename Y>
inline bool
operator<=(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	return !(rhs < lhs);
}

/**
 * Greater than operator.
 *
 * See less than operator for comparison rules.
 */
template <typename T, typename Y>
inline bool
operator>(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	return (rhs < lhs);
}

/**
 * Greater or equal than operator.
 *
 * See less than operator for comparison rules.
 */
template <typename T, typename Y>
inline bool
operator>=(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs) noexcept
{
	return !(lhs < rhs);
}

/* nullptr comparisons */

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator<(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return std::less<typename persistent_ptr<T>::element_type *>()(
		lhs.get(), nullptr);
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator<(std::nullptr_t, persistent_ptr<T> const &rhs) noexcept
{
	return std::less<typename persistent_ptr<T>::element_type *>()(
		nullptr, rhs.get());
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator<=(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return !(nullptr < lhs);
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator<=(std::nullptr_t, persistent_ptr<T> const &rhs) noexcept
{
	return !(rhs < nullptr);
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator>(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return nullptr < lhs;
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator>(std::nullptr_t, persistent_ptr<T> const &rhs) noexcept
{
	return rhs < nullptr;
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator>=(persistent_ptr<T> const &lhs, std::nullptr_t) noexcept
{
	return !(lhs < nullptr);
}

/**
 * Compare a persistent_ptr with a null pointer.
 */
template <typename T>
inline bool
operator>=(std::nullptr_t, persistent_ptr<T> const &rhs) noexcept
{
	return !(nullptr < rhs);
}

/**
 * Addition operator for persistent pointers.
 */
template <typename T>
inline persistent_ptr<T>
operator+(persistent_ptr<T> const &lhs, std::ptrdiff_t s)
{
	PMEMoid noid;
	noid.pool_uuid_lo = lhs.raw().pool_uuid_lo;
	noid.off = lhs.raw().off + (s * sizeof(T));
	return persistent_ptr<T>(noid);
}

/**
 * Subtraction operator for persistent pointers.
 */
template <typename T>
inline persistent_ptr<T>
operator-(persistent_ptr<T> const &lhs, std::ptrdiff_t s)
{
	PMEMoid noid;
	noid.pool_uuid_lo = lhs.raw().pool_uuid_lo;
	noid.off = lhs.raw().off - (s * sizeof(T));
	return persistent_ptr<T>(noid);
}

/**
 * Subtraction operator for persistent pointers of identical type.
 *
 * Calculates the offset difference of PMEMoids in terms of represented
 * objects. Calculating the difference of pointers from objects of
 * different pools is not allowed.
 */
template <typename T, typename Y,
	  typename = typename std::enable_if<
		  std::is_same<typename std::remove_cv<T>::type,
			       typename std::remove_cv<Y>::type>::value>>
inline ptrdiff_t
operator-(persistent_ptr<T> const &lhs, persistent_ptr<Y> const &rhs)
{
	assert(lhs.raw().pool_uuid_lo == rhs.raw().pool_uuid_lo);
	ptrdiff_t d = lhs.raw().off - rhs.raw().off;

	return d / sizeof(T);
}

/**
 * Ostream operator for the persistent pointer.
 */
template <typename T>
std::ostream &
operator<<(std::ostream &os, persistent_ptr<T> const &pptr)
{
	PMEMoid raw_oid = pptr.raw();
	os << std::hex << "0x" << raw_oid.pool_uuid_lo << ", 0x" << raw_oid.off
	   << std::dec;
	return os;
}

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_PERSISTENT_PTR_HPP */
