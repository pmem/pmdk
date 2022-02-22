# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

# membuf test

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/membuf)

execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/membuf)

cleanup()
