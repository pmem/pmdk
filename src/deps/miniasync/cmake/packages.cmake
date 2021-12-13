#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021, Intel Corporation
#

#
# packages.cmake - CPack configuration for rpm and deb generation
#

string(TOUPPER "${CPACK_GENERATOR}" CPACK_GENERATOR)

if(NOT ("${CPACK_GENERATOR}" STREQUAL "DEB" OR
	"${CPACK_GENERATOR}" STREQUAL "RPM"))
	message(FATAL_ERROR "Wrong CPACK_GENERATOR value, valid generators are: DEB, RPM")
endif()

if("${CPACK_GENERATOR}" STREQUAL "RPM")
	find_program(RPMBUILD NAMES rpmbuild)
	if(NOT RPMBUILD)
		message(FATAL_ERROR "rpmbuild not found - RPM packages will not be generated")
	endif()
endif()

if("${CPACK_GENERATOR}" STREQUAL "DEB")
	find_program(DEBBUILD NAMES dpkg-buildpackage)
	if(NOT DEBBUILD)
		message(FATAL_ERROR "dpkg-buildpackage not found - DEB packages will not be generated")
	endif()
endif()

set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CMAKE_INSTALL_TMPDIR /tmp CACHE PATH "Output dir for tmp")
set(CPACK_COMPONENTS_ALL_IN_ONE)

# Filter out some of directories from %dir section, which are expected
# to exist in filesystem. Leaving them might lead to conflicts with other
# packages (for example with 'filesystem' package on fedora which specify
# /usr, /usr/local, etc.)
set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
	${CPACK_PACKAGING_INSTALL_PREFIX}
	${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}
	${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig
	${CPACK_PACKAGING_INSTALL_PREFIX}/${CMAKE_INSTALL_INCDIR}
	${CPACK_PACKAGING_INSTALL_PREFIX}/share
	${CPACK_PACKAGING_INSTALL_PREFIX}/share/doc)

set(CPACK_PACKAGE_NAME "libminiasync")
set(CPACK_PACKAGE_VERSION ${VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "C low-level concurrency library for asynchronous functions")
set(CPACK_PACKAGE_VENDOR "Intel")

set(CPACK_RPM_PACKAGE_NAME "libminiasync-devel")
set(CPACK_RPM_PACKAGE_GROUP "Development/Libraries")
set(CPACK_RPM_PACKAGE_LICENSE "BSD")
set(CPACK_RPM_PACKAGE_ARCHITECTURE x86_64)
#set(CPACK_RPM_PACKAGE_REQUIRES "libexample")

set(CPACK_DEBIAN_PACKAGE_NAME "libminiasync-dev")
set(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION})
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libexample-dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "weronika.lewandowska@intel.com")

if("${CPACK_GENERATOR}" STREQUAL "RPM")
	set(CPACK_PACKAGE_FILE_NAME
		${CPACK_RPM_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}.${CPACK_RPM_PACKAGE_ARCHITECTURE})
elseif("${CPACK_GENERATOR}" STREQUAL "DEB")
	# We are using "gnutar" to avoid this bug:
	# https://gitlab.kitware.com/cmake/cmake/issues/14332
	set(CPACK_DEBIAN_ARCHIVE_TYPE "gnutar")
	set(CPACK_PACKAGE_FILE_NAME
		${CPACK_DEBIAN_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE})
endif()

set(targetDestDir ${CMAKE_INSTALL_TMPDIR})
include(CPack)
