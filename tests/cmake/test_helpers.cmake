# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation

cmake_minimum_required(VERSION 3.3)

set(DIR ${PARENT_DIR})
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/)
set(EXAMPLES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../examples)

function(setup)
	execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${BIN_DIR})
	execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_DIR})
endfunction()

function(cleanup)
	execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${DIR})
endfunction()

# execute_arg-- function executes test command ${name} and verifies its status matches ${expectation}.
#		Optional function arguments are passed as consecutive arguments to the command.
function(execute_arg input expectation name)
	message(STATUS "Executing: ${name} ${ARGN}")
	if("${input}" STREQUAL "")
		execute_process(COMMAND ${name} ${ARGN}
			RESULT_VARIABLE RET
			OUTPUT_FILE ${BIN_DIR}/out
			ERROR_FILE ${BIN_DIR}/err)
	else()
		execute_process(COMMAND ${name} ${ARGN}
			RESULT_VARIABLE RET
			INPUT_FILE ${input}
			OUTPUT_FILE ${BIN_DIR}/out
			ERROR_FILE ${BIN_DIR}/err)
	endif()
	message(STATUS "Test ${name}:")
	file(READ ${BIN_DIR}/out OUT)
	message(STATUS "Stdout:\n${OUT}")
	file(READ ${BIN_DIR}/err ERR)
	message(STATUS "Stderr:\n${ERR}")

	if(NOT RET EQUAL expectation)
		message(FATAL_ERROR "${name} ${ARGN} exit code ${RET} doesn't match expectation ${expectation}")
	endif()
endfunction()

# run_under_valgrind-- function executes test command ${name} under valgrind.
#		Optional function arguments are passed as consecutive arguments to the command.
function(run_under_valgrind vg_opt name)
	message(STATUS "Executing: valgrind ${vg_opt} ${name} ${ARGN}")
	execute_process(COMMAND valgrind ${vg_opt} ${name} ${ARGN}
			RESULT_VARIABLE RET
			OUTPUT_FILE ${BIN_DIR}/out
			ERROR_FILE ${BIN_DIR}/err)
	message(STATUS "Test ${name}:")
	file(READ ${BIN_DIR}/out OUT)
	message(STATUS "Stdout:\n${OUT}")
	file(READ ${BIN_DIR}/err ERR)
	message(STATUS "Stderr:\n${ERR}")

	if(NOT RET EQUAL 0)
		message(FATAL_ERROR
			"command 'valgrind ${name} ${ARGN}' failed:\n${ERR}")
	endif()

	set(text_passed "ERROR SUMMARY: 0 errors from 0 contexts")
	string(FIND "${ERR}" "${text_passed}" RET)
	if(RET EQUAL -1)
		message(FATAL_ERROR
			"command 'valgrind ${name} ${ARGN}' failed:\n${ERR}")
	endif()
endfunction()

# execute-- function calls the other, correct function for executing the test
#		based on the provided arguments.
function(execute expectation name)
	set(ENV{MINIASYNC_FILE} "${BIN_DIR}/out.log")
	set(ENV{MINIASYNC_LEVEL} "3")

	if (${TRACER} STREQUAL "none")
		execute_arg("" ${expectation} ${name} ${ARGN})
	elseif (${TRACER} STREQUAL memcheck)
		set(VG_OPT "--leak-check=full")
		run_under_valgrind("${VG_OPT}" ${name} ${ARGN})
	elseif (${TRACER} STREQUAL helgrind)
		set(HEL_SUPP "${SRC_DIR}/helgrind-log.supp")
		set(VG_OPT "--tool=helgrind" "--suppressions=${HEL_SUPP}")
		run_under_valgrind("${VG_OPT}" ${name} ${ARGN})
	elseif (${TRACER} STREQUAL drd)
		set(DRD_SUPP "${SRC_DIR}/drd-log.supp")
		set(VG_OPT "--tool=drd" "--suppressions=${DRD_SUPP}")
		run_under_valgrind("${VG_OPT}" ${name} ${ARGN})
	else ()
		message(FATAL_ERROR "unknown tracer: ${TRACER}")
	endif ()
endfunction()

# execute_assert_fail-- function executes test and asserts that its return code
#		is different than zero. If tracer is already used, the test is skipped.
function(execute_assert_fail name)
	set(ENV{MINIASYNC_FILE} "${BIN_DIR}/out.log")
	set(ENV{MINIASYNC_LEVEL} "3")

	if (NOT ${TRACER} STREQUAL "none")
		message(STATUS
			"tracer cannot be used with assert fail, skipping test ${name}")
			return()
	else ()
		message(STATUS "Executing: ${name} ${ARGN}")
		execute_process(COMMAND ${name} ${ARGN}
			RESULT_VARIABLE RET
			OUTPUT_FILE ${BIN_DIR}/out
			ERROR_FILE ${BIN_DIR}/err)
		message(STATUS "Test ${name}:")
		file(READ ${BIN_DIR}/out OUT)
		message(STATUS "Stdout:\n${OUT}")
		file(READ ${BIN_DIR}/err ERR)
		message(STATUS "Stderr:\n${ERR}")

		if(RET EQUAL 0)
			message(FATAL_ERROR "${name} ${ARGN} exit code ${RET} doesn't assert failure")
		endif()
	endif()
endfunction()

# execute_assert_pass-- function executes test and asserts that its return code
#		equals zero. If tracer is already used, the test is skipped.
function(execute_assert_pass name)
	set(ENV{MINIASYNC_FILE} "${BIN_DIR}/out.log")
	set(ENV{MINIASYNC_LEVEL} "3")

	if (NOT ${TRACER} STREQUAL "none")
		message(STATUS
			"tracer cannot be used with assert pass, skipping test ${name}")
			return()
	else ()
		message(STATUS "Executing: ${name} ${ARGN}")
		execute_process(COMMAND ${name} ${ARGN}
			RESULT_VARIABLE RET
			OUTPUT_FILE ${BIN_DIR}/out
			ERROR_FILE ${BIN_DIR}/err)
		message(STATUS "Test ${name}:")
		file(READ ${BIN_DIR}/out OUT)
		message(STATUS "Stdout:\n${OUT}")
		file(READ ${BIN_DIR}/err ERR)
		message(STATUS "Stderr:\n${ERR}")

		if(NOT RET EQUAL 0)
			message(FATAL_ERROR "${name} ${ARGN} exit code ${RET} doesn't assert pass")
		endif()
	endif()
endfunction()
