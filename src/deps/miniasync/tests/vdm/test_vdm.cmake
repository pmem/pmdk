# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# an example for the basic test case

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# The expected input can be provided to the execute function,
# which will check if the return code of the binary file matches.
execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm)

# execute_assert_pass can be used instead of manually checking if
# the return code equals zero, however it cannot be used with a tracer.
# When used with a tracer assert_pass will be skipped.
execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm)

cleanup()
