#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

add_custom_target(examples)

# add compiler flags using macro defined in functions.cmake file
add_flag(-Wall)
add_flag(-Wpointer-arith)
add_flag(-Wsign-compare)
add_flag(-Wunreachable-code-return)
add_flag(-Wmissing-variable-declarations)
add_flag(-fno-common)
add_flag(-Wunused-macros)
add_flag(-Wsign-conversion)

add_flag(-ggdb DEBUG)
add_flag(-DDEBUG DEBUG)

add_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

add_cstyle(examples-all ${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch])
add_check_whitespace(examples-all
		${CMAKE_CURRENT_SOURCE_DIR}/*/*.[ch]
		${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
		${CMAKE_CURRENT_SOURCE_DIR}/README.md)

# add_example-- function for adding an example.
#		After the required name parameter, next parameters
#		passed to this function are example's sources.
function(add_example name)
	include_directories(
		${MINIASYNC_SOURCE_DIR}
		${MINIASYNC_INCLUDE_DIR})
	set(srcs ${ARGN})
	prepend(srcs ${CMAKE_CURRENT_SOURCE_DIR} ${srcs})
	add_executable(example-${name} ${srcs})
	target_link_libraries(example-${name} miniasync ${CMAKE_THREAD_LIBS_INIT})
	add_dependencies(examples example-${name})
endfunction()

# add all the examples with a use of the add_example function defined above
add_example(basic basic/basic.c)
add_example(basic-async basic-async/basic-async.c)
