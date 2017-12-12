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
 * Commonly used conversions.
 */

#ifndef PMEMOBJ_CONVERSIONS_HPP
#define PMEMOBJ_CONVERSIONS_HPP

#include <chrono>
#include <time.h>

namespace nvml
{

namespace detail
{

/**
 * Convert std::chrono::time_point to posix timespec.
 *
 * @param[in] timepoint point in time to be converted.
 *
 * @return converted timespec structure.
 */
template <typename Clock, typename Duration = typename Clock::duration>
timespec
timepoint_to_timespec(const std::chrono::time_point<Clock, Duration> &timepoint)
{
	timespec ts;
	auto rel_duration = timepoint.time_since_epoch();
	const auto sec =
		std::chrono::duration_cast<std::chrono::seconds>(rel_duration);

	ts.tv_sec = sec.count();
	ts.tv_nsec = static_cast<long>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			rel_duration - sec)
			.count());

	return ts;
}

} /* namespace detail */

} /* namespace nvml */

#endif /* PMEMOBJ_CONVERSIONS_HPP */
