# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

set(GLOBAL_TEST_ARGS -DPARENT_DIR=${TEST_DIR})

if(TRACE_TESTS)
	set(GLOBAL_TEST_ARGS ${GLOBAL_TEST_ARGS} --trace-expand)
endif()

# add and link an executable
function(add_link_executable name sources libs)
	add_executable(${name} ${sources})

	target_include_directories(${name}
		PRIVATE ${MINIASYNC_SOURCE_DIR}
		${MINIASYNC_INCLUDE_DIR})

	target_include_directories(${name}
		PRIVATE ${MINIASYNC_DML_SOURCE_DIR}
		${MINIASYNC_DML_INCLUDE_DIR}
		${MINIASYNC_DML_SOURCE_DIR}/utils)

	target_include_directories(${name}
		PRIVATE ${CORE_SOURCE_DIR})

	target_link_libraries(${name} PRIVATE ${libs})
endfunction()

set(vg_tracers memcheck helgrind drd)

# test-- function for adding the test.
#		The first argument is a test name, which must be unique.
#		The second argument is a directory, which contains the .cmake file.
#		The third argument is a name of the .cmake file used for the test.
#		The last argument is a name of the tracer from the available list-
#		"memcheck", "helgrind", "drd" or ,in case of no tracer used, "none".
function(test name dir_name file tracer)
	if (${tracer} IN_LIST vg_tracers)
			if (NOT TESTS_USE_VALGRIND)
				message(STATUS
					"tests using valgrind are switched off, skipping valgrind test: ${name}")
				return()
			endif()
			if (NOT VALGRIND_FOUND)
				message(WARNING
					"valgrind not found, skipping valgrind test: ${name}")
				return()
			endif()
			if (COVERAGE_BUILD)
				message(STATUS
					"this is the coverage build, skipping valgrind test: ${name}")
				return()
			endif()
			if (USE_ASAN OR USE_UBSAN)
				message(STATUS
					"sanitizer used, skipping valgrind test: ${name}")
				return()
			endif()
	endif()
	add_test(NAME ${name}
		COMMAND ${CMAKE_COMMAND}
		${GLOBAL_TEST_ARGS}
		-DTEST_NAME=${name}
		-DSRC_DIR=${CMAKE_CURRENT_SOURCE_DIR}
		-DBIN_DIR=${CMAKE_CURRENT_BINARY_DIR}/${file}_${tracer}
		-DCONFIG=$<CONFIG>
		-DTRACER=${tracer}
		-DBUILD=${CMAKE_BUILD_TYPE}
		-DCMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
		-P ${CMAKE_CURRENT_SOURCE_DIR}/${dir_name}/${file}.cmake)

	set_tests_properties(${name} PROPERTIES
		ENVIRONMENT "LC_ALL=C;PATH=$ENV{PATH}"
		TIMEOUT 300)
endfunction()
