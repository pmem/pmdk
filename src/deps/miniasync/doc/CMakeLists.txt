# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2022, Intel Corporation

#
# doc/CMakeLists.txt - prepares 'make doc' command for documentation
#

# Converts md files into manpage format. Requires pandoc command.
# NAME is output manpage name, INPUT is a path to the source md file
function(configure_man NAME INPUT)
	add_custom_command(OUTPUT ${MAN_DIR}/${NAME}
		MAIN_DEPENDENCY ${INPUT}
		COMMAND ${MINIASYNC_ROOT_DIR}/utils/md2man/md2man.sh
			${INPUT}
			${MINIASYNC_ROOT_DIR}/utils/md2man/default.man
			${MAN_DIR}/${NAME}
			${VERSION})
	list(APPEND MANPAGE_OUTFILES ${MAN_DIR}/${NAME})
	set(MANPAGE_OUTFILES ${MANPAGE_OUTFILES} PARENT_SCOPE)
endfunction()

# Generate files (to be installed) - links of C API functions (passed as {ARGN})
# to chosen (3) manpage.
function(add_manpage_links MANPAGE)
	foreach(function ${ARGN})
		set(CONTENT ".so ${MANPAGE}")
		file(WRITE ${MAN_DIR}/${function}.3 "${CONTENT}")
	endforeach()
endfunction()

# ----------------------------------------------------------------- #
## Setup custom targets and useful variables
# ----------------------------------------------------------------- #
set(MANPAGE_OUTFILES "")
set(MAN_DIR ${CMAKE_CURRENT_BINARY_DIR}/man)
add_check_whitespace(man ${CMAKE_CURRENT_SOURCE_DIR}/*.*)

# ----------------------------------------------------------------- #
## Prepare C documentation (using manpage format)
# ----------------------------------------------------------------- #
find_program(PANDOC NAMES pandoc)
if(PANDOC)
	message(STATUS "Found pandoc: ${PANDOC}")

	## Manpages without any examples
	# miniasync.7
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync.7.md.in
		${MAN_DIR}/tmp/miniasync.7.md)
	configure_man(miniasync.7 ${MAN_DIR}/tmp/miniasync.7.md)

	# miniasync_future.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_future.3.md.in
		${MAN_DIR}/tmp/miniasync_future.3.md)
	configure_man(miniasync_future.3 ${MAN_DIR}/tmp/miniasync_future.3.md)
	add_manpage_links(miniasync_future.3
		future_context_get_data future_context_get_output future_context_get_size
		FUTURE FUTURE_INIT FUTURE_AS_RUNNABLE FUTURE_OUTPUT FUTURE_CHAIN_ENTRY
		FUTURE_CHAIN_ENTRY_INIT FUTURE_BUSY_POLL FUTURE_CHAIN_INIT)

	# miniasync_runtime.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_runtime.3.md.in
		${MAN_DIR}/tmp/miniasync_runtime.3.md)
	configure_man(miniasync_runtime.3 ${MAN_DIR}/tmp/miniasync_runtime.3.md)
	add_manpage_links(miniasync_runtime.3
		runtime_new runtime_delete runtime_wait_multiple runtime_wait)

	# miniasync_vdm.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_vdm.3.md.in
		${MAN_DIR}/tmp/miniasync_vdm.3.md)
	configure_man(miniasync_vdm.3 ${MAN_DIR}/tmp/miniasync_vdm.3.md)
	add_manpage_links(miniasync_vdm.3 vdm_memcpy vdm_new vdm_delete)

	# miniasync_vdm_synchronous.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_vdm_synchronous.3.md.in
			${MAN_DIR}/tmp/miniasync_vdm_synchronous.3.md)
	configure_man(miniasync_vdm_synchronous.3 ${MAN_DIR}/tmp/miniasync_vdm_synchronous.3.md)
	add_manpage_links(miniasync_vdm_synchronous.3 vdm_descriptor_synchronous)

	# miniasync_vdm_threads.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_vdm_threads.3.md.in
			${MAN_DIR}/tmp/miniasync_vdm_threads.3.md)
	configure_man(miniasync_vdm_threads.3 ${MAN_DIR}/tmp/miniasync_vdm_threads.3.md)
	add_manpage_links(miniasync_vdm_threads.3
		vdm_descriptor_threads vdm_descriptor_threads_polled)

	# miniasync_vdm_dml.3
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miniasync_vdm_dml.3.md.in
	${MAN_DIR}/tmp/miniasync_vdm_dml.3.md)
	configure_man(miniasync_vdm_dml.3 ${MAN_DIR}/tmp/miniasync_vdm_dml.3.md)
	add_manpage_links(miniasync_vdm_dml.3
		vdm_descriptor_dml vdm_descriptor_dml_async)

	# install manpages
	install(FILES ${MAN_DIR}/miniasync.7
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man7)
	install(DIRECTORY ${MAN_DIR}/
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
		FILES_MATCHING
		PATTERN "*.3"
		PATTERN "tmp" EXCLUDE)

	add_custom_target(doc ALL
		DEPENDS ${MANPAGE_OUTFILES}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
else()
	message(WARNING "pandoc not found - man pages (C documentation) will not be generated"
			"If you wish to build them install pandoc, "
			"otherwise disable it using CMake option -DBUILD_DOC=OFF")
endif()
