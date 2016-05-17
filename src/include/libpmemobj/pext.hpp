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
 * Convenience extensions for the resides on pmem property template.
 */

#ifndef PMEMOBJ_PEXT_HPP
#define PMEMOBJ_PEXT_HPP

#include "libpmemobj/p.hpp"
#include <iostream>

namespace nvml
{

namespace obj
{

/**
 * Ostream operator overload.
 */
template <typename T>
std::ostream &
operator<<(std::ostream &os, const p<T> &pp)
{
	return os << pp.get_ro();
}

/**
 * Istream operator overload.
 */
template <typename T>
std::istream &
operator>>(std::istream &is, p<T> &pp)
{
	is >> pp.get_rw();
	return is;
}

/**
 * Prefix increment operator overload.
 */
template <typename T>
p<T> &operator++(p<T> &pp)
{
	++(pp.get_rw());
	return pp;
}

/**
 * Prefix decrement operator overload.
 */
template <typename T>
p<T> &operator--(p<T> &pp)
{
	--(pp.get_rw());
	return pp;
}

/**
 * Postfix increment operator overload.
 */
template <typename T>
p<T> operator++(p<T> &pp, int)
{
	p<T> temp = pp;
	++pp;
	return temp;
}

/**
 * Postfix decrement operator overload.
 */
template <typename T>
p<T> operator--(p<T> &pp, int)
{
	p<T> temp = pp;
	--pp;
	return temp;
}

/**
 * Addition assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator+=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() += rhs.get_ro();
	return lhs;
}

/**
 * Addition assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator+=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() += rhs;
	return lhs;
}

/**
 * Subtraction assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator-=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() -= rhs.get_ro();
	return lhs;
}

/**
 * Subtraction assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator-=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() -= rhs;
	return lhs;
}

/**
 * Multiplication assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator*=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() *= rhs.get_ro();
	return lhs;
}

/**
 * Multiplication assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator*=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() *= rhs;
	return lhs;
}

/**
 * Division assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator/=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() /= rhs.get_ro();
	return lhs;
}

/**
 * Division assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator/=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() /= rhs;
	return lhs;
}

/**
 * Modulo assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator%=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() %= rhs.get_ro();
	return lhs;
}

/**
 * Modulo assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator%=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() %= rhs;
	return lhs;
}

/**
 * Bitwise AND assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator&=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() &= rhs.get_ro();
	return lhs;
}

/**
 * Bitwise AND assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator&=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() &= rhs;
	return lhs;
}

/**
 * Bitwise OR assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator|=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() |= rhs.get_ro();
	return lhs;
}

/**
 * Bitwise OR assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator|=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() |= rhs;
	return lhs;
}

/**
 * Bitwise XOR assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator^=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() ^= rhs.get_ro();
	return lhs;
}

/**
 * Bitwise XOR assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator^=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() ^= rhs;
	return lhs;
}

/**
 * Bitwise left shift assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator<<=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() = lhs.get_ro() << rhs.get_ro();
	return lhs;
}

/**
 * Bitwise left shift assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator<<=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() = lhs.get_ro() << rhs;
	return lhs;
}

/**
 * Bitwise right shift assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator>>=(p<T> &lhs, const p<Y> &rhs)
{
	lhs.get_rw() = lhs.get_ro() >> rhs.get_ro();
	return lhs;
}

/**
 * Bitwise right shift assignment operator overload.
 */
template <typename T, typename Y>
p<T> &
operator>>=(p<T> &lhs, const Y &rhs)
{
	lhs.get_rw() = lhs.get_ro() >> rhs;
	return lhs;
}

} /* namespace obj */

} /* namespace nvml */

#endif /* PMEMOBJ_PEXT_HPP */
