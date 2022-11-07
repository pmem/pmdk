# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# test for the future type

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/future)

execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/future)

cleanup()
