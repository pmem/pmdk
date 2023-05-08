#!usr/bin/env python3
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation

"""
This module includes tests for PMDK packages installation.
Tests check:
-compatibility of the version of installed  packages from PMDK library with
 the current version of PMDK library,
-if all packages from PMDK library are installed.
Required arguments:
-r <PMDK_path>    the PMDK library root path.
"""

from os import listdir, path, linesep, walk
from subprocess import check_output
import distro
import unittest
import xmlrunner
import sys
import re

NO_PKG_CONFIGS = ('pmdk', 'pmempool', 'daxio', 'pmreorder', 'rpmemd')
PMDK_TOOLS = ('pmempool', 'daxio', 'pmreorder', 'rpmemd')
PMDK_VERSION = ''
SYSTEM_ARCHITECTURE = ''
PMDK_PATH = ''


def get_package_version_and_system_architecture():
    """
    Returns packages version and system architecture from names of directories
    from pkg directory.
    """
    global PMDK_VERSION
    global SYSTEM_ARCHITECTURE
    os_distro = distro.id()
    if os_distro != 'ubuntu':
        pkg_directory = path.join(PMDK_PATH, 'rpm')
        for elem in listdir(pkg_directory):
            if '.src.rpm' in elem:
                # looks for the version number of package in package name
                PMDK_VERSION = re.search(
                    r'[\s]*pmdk-([\S]+).src.rpm', elem).group(1)
            else:
                    SYSTEM_ARCHITECTURE = elem

    elif os_distro == 'ubuntu':
        pkg_directory = path.join(PMDK_PATH, 'dpkg')
        for elem in listdir(pkg_directory):
            if '.changes' in elem:
                # looks for the version number of package in package name
                PMDK_VERSION = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(1)
                SYSTEM_ARCHITECTURE = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(2)


def get_libraries_names(packages_path, split_param):
    """
    Returns names of elements, for which are installed packages from PMDK
    library.
    """
    libraries_names = [re.split(split_param, elem)[0] for elem in listdir(packages_path)
                           if PMDK_VERSION in elem]

    return set(libraries_names)


def get_not_installed_packages(packages_path, so_path, split_param):
    """
    Returns names of packages from PMDK library, which are not installed.
    """
    def is_installed(elem):
        return elem in PMDK_TOOLS and elem in listdir('/usr/bin/') or\
            elem == "pmdk" or elem + '.so' in listdir(so_path)

    elements = get_libraries_names(packages_path, split_param)
    not_installed_packages = []
    for elem in elements:
        if not is_installed(elem):
            not_installed_packages.append(elem)
    return not_installed_packages


def get_incompatible_packages(packages_path, pkgconfig_directory, split_param):
    """
    Returns names of packages from PMDK library, which are not compatible
    with the current version of PMDK library.
    """
    incompatibe_packages = []
    libraries = get_libraries_names(packages_path, split_param) - set(NO_PKG_CONFIGS)
    for library in libraries:
        with open(pkgconfig_directory + library + '.pc') as f:
            out = f.readlines()
        for line in out:
            if 'version=' in line:
                version = line.split('=')[1].strip(linesep)
        if not version in PMDK_VERSION.replace('~', '-'):
            incompatibe_packages.append(library)
    return incompatibe_packages


class TestBuildPackages(unittest.TestCase):

    def test_compatibility_of_version_of_installed_packages(self):
        """
        Checks if the version of installed packages is correct.
        """
        incompatible_packages = get_incompatible_packages(packages_path, pkgconfig_directory, split_param)
        error_msg = linesep + 'List of incompatible packages: '
        for package in incompatible_packages:
            error_msg += linesep + package
        self.assertFalse(incompatible_packages, error_msg)

    def test_correctness_of_installed_packages(self):
        """
        Checks if all packages from PMDK library are installed.
        """
        not_installed_packages = get_not_installed_packages(packages_path, so_path, split_param)
        error_msg = linesep + 'List of not installed packages: '
        for package in not_installed_packages:
            error_msg += linesep + package
        self.assertFalse(not_installed_packages, error_msg)


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


if __name__ == '__main__':

    if '-h' in sys.argv or '--help' in sys.argv:
        print(__doc__)
        unittest.main()
    elif '-r' in sys.argv:
        PMDK_PATH = parse_argument('-r')
        get_package_version_and_system_architecture()
        os_distro = distro.id()
        if os_distro != 'ubuntu':
            packages_path = path.join(PMDK_PATH, 'rpm', SYSTEM_ARCHITECTURE)
            pkgconfig_directory = '/usr/lib64/pkgconfig/'
            so_path = '/usr/lib64/'
            split_param = '-'
        elif os_distro == 'ubuntu':
            packages_path = path.join(PMDK_PATH, 'dpkg')
            pkgconfig_directory = '/lib/x86_64-linux-gnu/pkgconfig/'
            so_path = '/lib/x86_64-linux-gnu/'
            split_param = '[-_]'

        if PMDK_VERSION == '' or SYSTEM_ARCHITECTURE == '':
            sys.exit("FATAL ERROR: command 'make rpm/dpkg' was not done correctly")
        unittest.main(
                testRunner=xmlrunner.XMLTestRunner(output='test-reports'),
                # these make sure that some options that are not applicable
                # remain hidden from the help menu.
                failfast=False, buffer=False, catchbreak=False)
    else:
        print(__doc__)
        print(unittest.main.__doc__)
