# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# check for MOVDIR64B instruction
check_movdir64b()

# execute DML tests only if MOVDIR64B instruction is available
if (MOVDIR64B EQUAL 1)
    execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/vdm_operation_future_poll)
endif()

cleanup()
