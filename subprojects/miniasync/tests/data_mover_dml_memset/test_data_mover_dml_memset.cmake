# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, Intel Corporation

# an example for the data_mover_dml test case

include(${SRC_DIR}/cmake/test_helpers.cmake)

setup()

# check for MOVDIR64B instruction
check_movdir64b()

# inform that some test cases involving 'mvodir64b' instruction will be skipped
if (MOVDIR64B EQUAL 0)
	message(STATUS "movdir64b instruction not available, some test cases will be skipped!")
endif()

execute(0 ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/data_mover_dml_memset)

execute_assert_pass(${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${BUILD}/data_mover_dml_memset)

cleanup()
