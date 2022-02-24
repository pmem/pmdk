#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

include(cmake/ctest_helpers.cmake)

set(LIBS_BASIC
	miniasync
	cores
	${CMAKE_THREAD_LIBS_INIT})

set(LIBS_DML
	miniasync-dml
	dl
	dml
	${LIBS_BASIC})

set(SOURCES_DUMMY_TEST
	dummy/dummy.c)

set(SOURCES_DUMMY_NEGATIVE_TEST
	dummy_negative/dummy_negative.c)

set(SOURCES_MEMCPY_PTHREADS_TEST
		memcpy_threads/memcpy_threads.c)

set(SOURCES_DATA_MOVER_DML_TEST
	data_mover_dml/data_mover_dml.c
	${MINIASYNC_DML_SOURCE_DIR}/utils/util_dml.c)

set(SOURCES_MEMBUF_TEST
	membuf/membuf_simple.c)

add_custom_target(tests)

add_flag(-Wall)

add_cstyle(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch])
add_check_whitespace(tests-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
		"${CMAKE_CURRENT_SOURCE_DIR}/*/*[.cmake]"
		"${CMAKE_CURRENT_SOURCE_DIR}/cmake/*[.cmake]")

include_directories(
		${MINIASYNC_SOURCE_DIR}
		${MINIASYNC_INCLUDE_DIR}
		${CMAKE_CURRENT_SOURCE_DIR})

# add and link all the executables for tests
add_link_executable(dummy
		"${SOURCES_DUMMY_TEST}"
		"${LIBS_BASIC}")

add_link_executable(dummy_negative
		"${SOURCES_DUMMY_NEGATIVE_TEST}"
		"${LIBS_BASIC}")

add_link_executable(memcpy_threads
		"${SOURCES_MEMCPY_PTHREADS_TEST}"
		"${LIBS_BASIC}")

add_link_executable(membuf
		"${SOURCES_MEMBUF_TEST}"
		"${LIBS_BASIC}")

# add test using test function defined in the ctest_helpers.cmake file
test("dummy" "dummy" test_dummy none)
test("dummy_drd" "dummy" test_dummy drd)
test("dummy_helgrind" "dummy" test_dummy helgrind)
test("dummy_memcheck" "dummy" test_dummy memcheck)
test("dummy_negative" "dummy_negative" test_dummy_negative none)
test("ex_basic" "ex_basic" test_ex_basic none)
test("ex_basic_async" "ex_basic_async" test_ex_basic_async none)
test("memcpy_threads" "memcpy_threads" test_memcpy_threads none)
test("membuf" "membuf" test_membuf none)

# add miniasync-dml test only if the sources in extras/dml were compiled
if (COMPILE_DML)
	add_link_executable(data_mover_dml
			"${SOURCES_DATA_MOVER_DML_TEST}"
			"${LIBS_DML}")

	test("data_mover_dml" "data_mover_dml" test_data_mover_dml none)
endif()
