#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# functions.cmake - helper functions for CMakeLists.txt
#

# prepend-- function prepends prefix to list of strings.
function(prepend var prefix)
	set(listVar "")
	foreach(f ${ARGN})
		list(APPEND listVar "${prefix}/${f}")
	endforeach(f)
	set(${var} "${listVar}" PARENT_SCOPE)
endfunction()

# add_flag-- macro checks whether flag is supported by current C compiler
#		and appends it to the relevant cmake variable.
#		1st argument is a flag.
#		2nd (optional) argument is a build type (debug, release).
macro(add_flag flag)
	string(REPLACE - _ flag2 ${flag})
	string(REPLACE " " _ flag2 ${flag2})
	string(REPLACE = "_" flag2 ${flag2})
	set(check_name "C_HAS_${flag2}")

	check_c_compiler_flag(${flag} ${check_name})

	if (${${check_name}})
		if (${ARGC} EQUAL 1)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
		else()
			set(CMAKE_C_FLAGS_${ARGV1} "${CMAKE_C_FLAGS_${ARGV1}} ${flag}")
		endif()
	endif()
endmacro()

macro(add_sanitizer_flag flag)
	set(SAVED_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})
	set(CMAKE_REQUIRED_LIBRARIES "${CMAKE_REQUIRED_LIBRARIES} -fsanitize=${flag}")

	if(${flag} STREQUAL "address")
		set(check_name "C_HAS_ASAN")
	elseif(${flag} STREQUAL "undefined")
		set(check_name "C_HAS_UBSAN")
	endif()

	check_c_compiler_flag("-fsanitize=${flag}" ${check_name})
	if (${${check_name}})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=${flag}")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${flag}")
	else()
		message(STATUS "  ${flag} sanitizer is not supported")
	endif()

	set(CMAKE_REQUIRED_LIBRARIES ${SAVED_CMAKE_REQUIRED_LIBRARIES})
endmacro()

# add_cstyle-- function generates cstyle-$name target and attaches it
#		as a dependency of global "cstyle" target.
#		cstyle-$name target verifies C style of files in current source dir.
#		If more arguments are used, they are used as files to be checked
#		instead.
#		${name} must be unique.
function(add_cstyle name)
	if(${ARGC} EQUAL 1)
		add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/cstyle-${name}-status
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c
				${CMAKE_CURRENT_SOURCE_DIR}/*.h
			COMMAND ${PERL_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/utils/cstyle -pP -o src2man
				${CMAKE_CURRENT_SOURCE_DIR}/*.c
				${CMAKE_CURRENT_SOURCE_DIR}/*.h
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/cstyle-${name}-status
			)
	else()
		add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/cstyle-${name}-status
			DEPENDS ${ARGN}
			COMMAND ${PERL_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/utils/cstyle -pP -o src2man
				${ARGN}
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/cstyle-${name}-status
			)
	endif()

	add_custom_target(cstyle-${name}
			DEPENDS ${CMAKE_BINARY_DIR}/cstyle-${name}-status)
	add_dependencies(cstyle cstyle-${name})
endfunction()

# add_check_whitespace-- function generates check-whitespace-$name target
#		and attaches it as a dependency of global "check-whitespace" target.
#		${name} must be unique.
function(add_check_whitespace name)
	if(${ARGC} EQUAL 1)
		add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/check-whitespace-${name}-status
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.c
				${CMAKE_CURRENT_SOURCE_DIR}/*.h
			COMMAND ${PERL_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/utils/check_whitespace
				${CMAKE_CURRENT_SOURCE_DIR}/*.c
				${CMAKE_CURRENT_SOURCE_DIR}/*.h
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/check-whitespace-${name}-status)
	else()
		add_custom_command(OUTPUT ${CMAKE_BINARY_DIR}/check-whitespace-${name}-status
			DEPENDS ${ARGN}
			COMMAND ${PERL_EXECUTABLE}
				${CMAKE_SOURCE_DIR}/utils/check_whitespace ${ARGN}
			COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_BINARY_DIR}/check-whitespace-${name}-status)
	endif()

	add_custom_target(check-whitespace-${name}
			DEPENDS ${CMAKE_BINARY_DIR}/check-whitespace-${name}-status)
	add_dependencies(check-whitespace check-whitespace-${name})
endfunction()
