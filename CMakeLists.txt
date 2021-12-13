#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

cmake_minimum_required(VERSION 3.3)

project(miniasync C)
set(MINIASYNC_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(MINIASYNC_SOURCE_DIR ${MINIASYNC_ROOT_DIR}/src)
set(MINIASYNC_INCLUDE_DIR ${MINIASYNC_ROOT_DIR}/src/include)
set(EXAMPLES_DIR ${MINIASYNC_ROOT_DIR}/examples/)

set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(TEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/test
	CACHE STRING "working directory for tests")

option(COVERAGE "run coverage test" OFF)
option(DEVELOPER_MODE "enable developer checks" OFF)
option(CHECK_CSTYLE "check code style of C sources" OFF)
option(TRACE_TESTS "more verbose test outputs" OFF)
option(USE_ASAN "enable AddressSanitizer (debugging)" OFF)
option(USE_UBSAN "enable UndefinedBehaviorSanitizer (debugging)" OFF)
option(BUILD_DOC "build documentation" ON)
option(BUILD_EXAMPLES "build examples" ON)
option(BUILD_TESTS "build tests" ON)
option(TESTS_USE_VALGRIND "enable tests with valgrind (if found)" ON)

include(FindPerl)
include(FindThreads)
include(CMakePackageConfigHelpers)
include(CheckCCompilerFlag)
include(GNUInstallDirs)
include(${CMAKE_SOURCE_DIR}/cmake/functions.cmake)

# look for pkg config (use it later for managing valgrind)
find_package(PkgConfig REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Debug")
endif()

if(NOT PERL_FOUND)
	message(FATAL_ERROR "Perl not found")
endif()

add_custom_target(checkers ALL)
add_custom_target(cstyle)
add_custom_target(check-whitespace)
add_custom_target(check-license
	COMMAND ${CMAKE_SOURCE_DIR}/utils/check_license/check-headers.sh
		${CMAKE_SOURCE_DIR}
		BSD-3-Clause)

add_custom_target(check-commits
	COMMAND ${CMAKE_SOURCE_DIR}/utils/check-commits.sh)

add_custom_target(check-whitespace-main
	COMMAND ${PERL_EXECUTABLE}
		${CMAKE_SOURCE_DIR}/utils/check_whitespace
		${CMAKE_SOURCE_DIR}/README.md
		${CMAKE_SOURCE_DIR}/LICENSE)

add_dependencies(check-whitespace check-whitespace-main)

# add compiler flags using macro defined in the functions.cmake file
add_flag(-Wpointer-arith)
add_flag(-Wunused-macros)
add_flag(-Wsign-conversion)
add_flag(-Wsign-compare)
add_flag(-Wunreachable-code-return)
add_flag(-Wmissing-variable-declarations)
add_flag(-fno-common)
add_flag(-std=gnu99)
add_flag(-ggdb DEBUG)
add_flag(-DDEBUG DEBUG)
add_flag("-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2" RELEASE)

if(USE_ASAN)
	add_sanitizer_flag(address)
endif()

if(USE_UBSAN)
	add_sanitizer_flag(undefined)
endif()

if(COVERAGE)
       set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -coverage")
endif()

if(DEVELOPER_MODE)
	add_flag(-Wall)
	add_flag(-Werror)

	if(CHECK_CSTYLE)
		add_dependencies(checkers cstyle)
	endif()
	add_dependencies(checkers check-whitespace)
	add_dependencies(checkers check-license)
	add_dependencies(checkers check-commits)
endif()

# add checkers for the root CMakeLists file and for cmake helper files
add_check_whitespace(cmake-main ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)
add_check_whitespace(cmake-helpers ${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake)

# add checkers for utils and subdirectories
add_check_whitespace(utils-all
		${CMAKE_CURRENT_SOURCE_DIR}/utils/*
		${CMAKE_CURRENT_SOURCE_DIR}/utils/check_license/*
		${CMAKE_CURRENT_SOURCE_DIR}/utils/docker/*
		${CMAKE_CURRENT_SOURCE_DIR}/utils/docker/images/*
		${CMAKE_CURRENT_SOURCE_DIR}/utils/md2man/*)

# look for and enable valgrind
pkg_check_modules(VALGRIND valgrind)

configure_file(${CMAKE_SOURCE_DIR}/cmake/libminiasync.pc.in
	${CMAKE_CURRENT_BINARY_DIR}/libminiasync.pc @ONLY)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libminiasync.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

configure_package_config_file(${CMAKE_SOURCE_DIR}/cmake/libminiasync-config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/libminiasync-config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/libminiasync/cmake
	PATH_VARS CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR)

write_basic_package_version_file(libminiasync-config-version.cmake
				VERSION ${VERSION}
				COMPATIBILITY AnyNewerVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libminiasync-config.cmake ${CMAKE_CURRENT_BINARY_DIR}/libminiasync-config-version.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/libminiasync/cmake)

if(VALGRIND_FOUND)
	add_flag(-DVG_MEMCHECK_ENABLED=1)
	add_flag(-DVG_DRD_ENABLED=1)
	add_flag(-DVG_HELGRIND_ENABLED=1)

	include_directories(${VALGRIND_INCLUDE_DIRS})
endif()

# add CMakeLists.txt from the src directory
add_subdirectory(src)

# add CMakeLists.txt from the tests directory
if(BUILD_TESTS)
	if(TEST_DIR)
		enable_testing()
	else()
		message(WARNING "TEST_DIR is empty - 'make test' will not work")
	endif()

	add_subdirectory(tests)
endif()

# add CMakeLists.txt from the examples directory
if(BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

# add CMakeLists.txt from the doc directory
if(BUILD_DOC)
	add_subdirectory(doc)
endif()

if(NOT "${CPACK_GENERATOR}" STREQUAL "")
	include(${CMAKE_SOURCE_DIR}/cmake/packages.cmake)
endif()
