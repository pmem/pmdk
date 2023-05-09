#!usr/bin/env python3
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation

"""
This module includes tests that check if all packages in PMDK library are
built correctly.
Tests check:
-if all required packages are built,
-if built packages are consistent with names of libraries read from
 .so files and other elements (tools and "PMDK").

Required arguments:
-r <PMDK_path>    the PMDK library root path.

Optional arguments:
--skip-daxio      to be set if daxio was not built (e.g., if NDCTL_ENABLE was set to 'n')
"""

from os import listdir, path, linesep
from collections import namedtuple
import distro
import unittest
import xmlrunner
import sys
import re

PACKAGES_INFO = namedtuple('packages_info',
                           'basic devel debug debuginfo debug_debuginfo')
PMDK_VERSION = ''
SYSTEM_ARCHITECTURE = ''


def get_package_version_and_system_architecture(pmdk_path):
    """
    Returns packages version and system architecture from names of directories
    from packages directory.
    """
    os_distro=distro.id()
    if os_distro != 'ubuntu':
        pkg_directory = path.join(pmdk_path, 'rpm')
    else:
        pkg_directory = path.join(pmdk_path, 'dpkg')

    version = ''
    architecture = ''
    for elem in listdir(pkg_directory):
        if os_distro != 'ubuntu':
            if '.src.rpm' in elem:
                # looks for the version number of rpm package in rpm package name
                version = re.search(r'[\s]*pmdk-([\S]+).src.rpm', elem).group(1)
            else:
                architecture = elem

        else:
            if '.changes' in elem:
                # looks for the version number of packages in package name
                version = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(1)
                architecture = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(2)
    return version, architecture


def get_built_packages(pmdk_pkg_path):
    """
    Returns built packages.
    """
    packages = listdir(pmdk_pkg_path)
    return packages


def get_libraries_names_from_so_files(pmdk_path, is_pmdk_debuginfo):
    """
    Returns names of libraries from .so files, and information which packages
    should be built for individual libraries.
    """
    libraries_from_so_files = dict()
    path_to_so_files = path.join(pmdk_path, 'src', 'nondebug')

    for elem in listdir(path_to_so_files):
        if elem.endswith('.so') and elem.startswith('lib'):
            library_name = elem.split('.')[0]
            if is_pmdk_debuginfo:
                libraries_from_so_files[library_name] =\
                    PACKAGES_INFO(basic=True, devel=True, debug=True,
                                  debuginfo=False, debug_debuginfo=False)
            else:
                libraries_from_so_files[library_name] =\
                    PACKAGES_INFO(basic=True, devel=True, debug=True,
                                  debuginfo=True, debug_debuginfo=True)
    return libraries_from_so_files


def get_names_of_packages(packages_info):
    """
    Returns names of packages, that should be built.
    """
    packages = []
    pkg_ext = ''
    separator = ''
    os_distro=distro.id()
    if os_distro != 'ubuntu':
        types = ['-', '-debug-', '-devel-', '-debuginfo-', '-debug-debuginfo-']
        pkg_ext = '.rpm'
        separator ='.'

    elif os_distro == 'ubuntu':
        types = ['_', '-dev_']
        pkg_ext = '.deb'
        separator = '_'

    for elem in packages_info:
        sets_of_information = zip(packages_info[elem], types)
        for kit in sets_of_information:
            if kit[0] and os_distro == 'opensuse':
                if elem.startswith('lib') and (kit[1] == '-' or kit[1] == '-debuginfo-'):
                    package_name = elem + str(1) + kit[1] + PMDK_VERSION + separator + \
                        SYSTEM_ARCHITECTURE + pkg_ext
                    packages.append(package_name)
                elif kit[0]:
                    package_name = elem + kit[1] + PMDK_VERSION + separator + \
                        SYSTEM_ARCHITECTURE + pkg_ext
                    packages.append(package_name)

            elif kit[0] and not os_distro == 'opensuse':
                package_name = elem + kit[1] + PMDK_VERSION + separator +\
                    SYSTEM_ARCHITECTURE + pkg_ext
                packages.append(package_name)
    return packages


def check_existence_of_pmdk_debuginfo_package(pmdk_debuginfo_package_name, built_packages):
    """
    Checks if 'pmdk-debuginfo' package is built
    """
    is_pmdk_debuginfo_package = False
    if pmdk_debuginfo_package_name in built_packages:
        is_pmdk_debuginfo_package = True
    return is_pmdk_debuginfo_package


def find_missing_packages(packages_path, pmdk_path, pmdk_debuginfo_package_name):
    """
    Checks if names of built packages are the same as names of packages,
    which should be built and returns missing packages. Tools are taken
    into account.
    """
    built_packages = get_built_packages(packages_path)
    is_pmdk_debuginfo =\
        check_existence_of_pmdk_debuginfo_package(pmdk_debuginfo_package_name, built_packages)
    tools = {
        'pmempool': PACKAGES_INFO(basic=True, devel=False, debug=False,
                                  debuginfo=True, debug_debuginfo=False),
        'pmreorder': PACKAGES_INFO(basic=True, devel=False, debug=False,
                                   debuginfo=False, debug_debuginfo=False)
    }
    if not skip_daxio:
        tools['daxio'] = PACKAGES_INFO(basic=True, devel=False, debug=False,
                                       debuginfo=True, debug_debuginfo=False)

    tools_packages = get_names_of_packages(tools)
    missing_tools_packages = [
        elem for elem in tools_packages if elem not in built_packages]
    libraries = get_libraries_names_from_so_files(pmdk_path, is_pmdk_debuginfo)
    library_packages = get_names_of_packages(libraries)
    missing_library_packages = [
        elem for elem in library_packages if elem not in built_packages]
    missing_packages = missing_library_packages + missing_tools_packages
    return missing_packages


def find_missing_libraries_and_tools(packages_path, pmdk_path, pmdk_debuginfo_package_name, library_name_pattern):
    """
    Checks if names of functions from .so files are the same as names of
    functions extracted from built packages and returns missing functions.
    Others pkg (tools and "PMDK") are taken into account.
    """
    others_pkg = ['pmempool', 'daxio', 'pmdk', 'pmreorder']
    built_packages = get_built_packages(packages_path)
    is_pmdk_debuginfo =\
        check_existence_of_pmdk_debuginfo_package(pmdk_debuginfo_package_name, built_packages)
    libraries = get_libraries_names_from_so_files(pmdk_path, is_pmdk_debuginfo)

    missing_elements = []
    # looks for the name of library/others in package name
    for elem in listdir(packages_path):
        library_name = re.search(library_name_pattern, elem).group(1)
        if library_name.endswith('1'):
            library_name = library_name.replace('1','')
        if library_name not in libraries.keys() and library_name not in\
                others_pkg and library_name not in missing_elements:
            missing_elements.append(library_name)
    return missing_elements


def parse_argument(argument_option):
    """
    Parses an option from the command line.
    """
    index = sys.argv.index(argument_option)
    try:
        argument = sys.argv[index+1]
    except IndexError:
        print('ERROR: Invalid argument!')
        print(__doc__)
        print(unittest.main.__doc__)
    else:
        sys.argv.pop(index)
        sys.argv.pop(index)
        return argument


class TestBuildPackages(unittest.TestCase):

    def test_completeness_of_built_packages(self):
        """
        Checks if all packages are built.
        """
        missing_packages =\
            find_missing_packages(packages_path, pmdk_path, pmdk_debuginfo_package_name)
        error_msg = linesep + 'List of missing packages:'

        for package in missing_packages:
            error_msg += linesep + package
        self.assertFalse(missing_packages, error_msg)

    def test_completeness_of_name_of_libraries_and_tools(self):
        """
        Checks if names of functions from .so files and other elements (tools
        and "PMDK") are the same as functions/other elements extracted from
        the name of built packages.
        """
        os_distro=distro.id()
        missing_elements =\
        find_missing_libraries_and_tools(packages_path, pmdk_path, pmdk_debuginfo_package_name,library_name_pattern)
        error_msg = linesep +\
            'List of missing libraries and other elements (tools and "PMDK"):'
        for elem in missing_elements:
            error_msg += linesep + elem

        self.assertFalse(missing_elements, error_msg)


if __name__ == '__main__':
    path_argument = '-r'
    daxio_build_argument = "--skip-daxio"
    skip_daxio = False
    if '-h' in sys.argv or '--help' in sys.argv:
        print(__doc__)
        unittest.main()
    elif path_argument in sys.argv:
        pmdk_path = parse_argument(path_argument)
        if daxio_build_argument in sys.argv:
            skip_daxio = True
            index = sys.argv.index(daxio_build_argument)
            sys.argv.pop(index)
        if pmdk_path:
            PMDK_VERSION, SYSTEM_ARCHITECTURE =\
                get_package_version_and_system_architecture(pmdk_path)
            os_distro=distro.id()
            if os_distro != 'ubuntu':
                packages_path = path.join(pmdk_path, 'rpm', SYSTEM_ARCHITECTURE)
                pmdk_debuginfo_package_name = 'pmdk-debuginfo-' + PMDK_VERSION + '.' + SYSTEM_ARCHITECTURE + '.rpm'
                library_name_pattern = r'[\s]*([2a-zA-Z+\d]+)-'
            else:
                packages_path = path.join(pmdk_path, 'dpkg')
                pmdk_debuginfo_package_name = 'pmdk-debuginfo-' + PMDK_VERSION + '.' + '.deb'
                library_name_pattern = r'^([2a-zA-Z]+)[-_].*$'
            unittest.main(
                testRunner=xmlrunner.XMLTestRunner(output='test-reports'),
                # these make sure that some options that are not applicable
                # remain hidden from the help menu.
                failfast=False, buffer=False, catchbreak=False)
    else:
        print(__doc__)
        print(unittest.main.__doc__)
