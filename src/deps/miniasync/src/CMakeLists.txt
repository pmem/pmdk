#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation
#

add_cstyle(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.[h]
	${CMAKE_CURRENT_SOURCE_DIR}/include/libminiasync/*.[h]
	${CMAKE_CURRENT_SOURCE_DIR}/windows/include/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.[ch])

add_check_whitespace(src
	${CMAKE_CURRENT_SOURCE_DIR}/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/include/*.[h]
	${CMAKE_CURRENT_SOURCE_DIR}/include/libminiasync/*.[h]
	${CMAKE_CURRENT_SOURCE_DIR}/windows/include/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/core/*.[ch]
	${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt)

set(SOURCES
    runtime.c
    data_mover_threads.c
    data_mover_sync.c
)

if(WIN32)
	set(CORE_DEPS
		${CORE_SOURCE_DIR}/os_windows.c
		${CORE_SOURCE_DIR}/os_thread_windows.c
		${CORE_SOURCE_DIR}/util_windows.c)
else()
	set(CORE_DEPS
		${CORE_SOURCE_DIR}/os_thread_posix.c
		${CORE_SOURCE_DIR}/os_posix.c
		${CORE_SOURCE_DIR}/util_posix.c)
endif()

set(CORE_DEPS ${CORE_DEPS}
	${CORE_SOURCE_DIR}/cpu.c
	${CORE_SOURCE_DIR}/membuf.c
	${CORE_SOURCE_DIR}/out.c
	${CORE_SOURCE_DIR}/util.c
	${CORE_SOURCE_DIR}/ringbuf.c)

add_library(cores STATIC ${CORE_DEPS})
add_library(miniasync SHARED ${SOURCES} miniasync.def)
set_property(TARGET cores PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(miniasync PRIVATE cores)

target_include_directories(miniasync PRIVATE . include)
target_include_directories(cores PRIVATE . include)

if(WIN32)
	target_include_directories(miniasync PRIVATE ${MINIASYNC_INCLUDE_DIR_WIN}/sys)
	target_include_directories(miniasync PRIVATE ${MINIASYNC_INCLUDE_DIR_WIN})
	target_include_directories(cores PRIVATE ${MINIASYNC_INCLUDE_DIR_WIN}/sys)
	target_include_directories(cores PRIVATE ${MINIASYNC_INCLUDE_DIR_WIN})
endif()

set_target_properties(miniasync PROPERTIES
	SOVERSION 0.1.0-rc1
	PUBLIC_HEADER ${MINIASYNC_INCLUDE_DIR}/libminiasync.h
)

install(TARGETS miniasync
	PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install headers included in public header
install(DIRECTORY ${MINIASYNC_INCLUDE_DIR}/libminiasync
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)
