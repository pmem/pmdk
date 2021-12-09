# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

# an example of the negative test case

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# compare expected exit code with the actual exit code
execute(1 ${TEST_DIR}/dummy_negative)

# If we don't know the exact expected exit code, we can use
# execute_assert_fail function, which checks if the
# exit code is different than zero, in which case,
# the test passes. This function cannot be used with a tracer.
# When used with a tracer assert_fail will be skipped.
execute_assert_fail(${TEST_DIR}/dummy_negative)

cleanup()
