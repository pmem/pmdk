# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# test the future poll operation with various data movers

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm_operation_future_poll)

execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm_operation_future_poll)

cleanup()
