#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

project(miniasync-dml C)

add_cstyle(miniasync-dml
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libminiasync-dml/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/utils/*.[ch])

add_check_whitespace(miniasync-dml
	${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
	${CMAKE_CURRENT_SOURCE_DIR}/README.md
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/include/libminiasync-dml/*.h
	${CMAKE_CURRENT_SOURCE_DIR}/utils/*.[ch])

set(DML_SOURCES
    data_mover_dml.c
)

set(DML_CORE_DEPS
	${CORE_SOURCE_DIR}/cpu.c
	${CORE_SOURCE_DIR}/os_posix.c
	${CORE_SOURCE_DIR}/os_thread_posix.c
	${CORE_SOURCE_DIR}/out.c
	${CORE_SOURCE_DIR}/util.c
	${CORE_SOURCE_DIR}/util_posix.c)

add_library(miniasync-dml SHARED ${DML_SOURCES} ${DML_CORE_DEPS})

target_include_directories(miniasync-dml PRIVATE .
	${MINIASYNC_DML_INCLUDE_DIR}
	${MINIASYNC_INCLUDE_DIR}
	${MINIASYNC_SOURCE_DIR})

set_target_properties(miniasync-dml PROPERTIES
	PUBLIC_HEADER ${MINIASYNC_DML_INCLUDE_DIR}/libminiasync-dml.h
)

install(TARGETS miniasync-dml
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers included in public header
install(DIRECTORY ${MINIASYNC_DML_INCLUDE_DIR}/libminiasync-dml
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	FILES_MATCHING PATTERN "*.h"
)

set(CHECK_MOVDIR64B_SOURCES
	${MINIASYNC_DML_SOURCE_DIR}/utils/check_movdir64b.c
	${CORE_SOURCE_DIR}/cpu.c)

add_link_executable(check_movdir64b
	"${CHECK_MOVDIR64B_SOURCES}"
	"")
