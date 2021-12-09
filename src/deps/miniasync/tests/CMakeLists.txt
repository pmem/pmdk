#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

include(cmake/ctest_helpers.cmake)

set(LIBS_BASIC
	miniasync
	${CMAKE_THREAD_LIBS_INIT})

set(SOURCES_DUMMY_TEST
	dummy/dummy.c)

set(SOURCES_DUMMY_NEGATIVE_TEST
	dummy_negative/dummy_negative.c)

add_custom_target(tests)

add_flag(-Wall)

add_cstyle(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch])
add_check_whitespace(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.cmake
		${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake)

# add and link all the executables for tests
add_link_executable(dummy
		"${SOURCES_DUMMY_TEST}"
        "${LIBS_BASIC}")

add_link_executable(dummy_negative
		"${SOURCES_DUMMY_NEGATIVE_TEST}"
        "${LIBS_BASIC}")

# add test using test function defined in the ctest_helpers.cmake file
test("dummy_memcheck" "dummy" test_dummy memcheck)
test("dummy" "dummy" test_dummy none)
test("dummy_negative" "dummy_negative" test_dummy_negative none)
test("ex_basic" "ex_basic" test_ex_basic none)
