#!usr/bin/env python3
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation

"""
This module includes functions which install packages from PMDK library.
"""

from os import listdir, path, linesep
from subprocess import check_output
from argparse import ArgumentParser
import distro
import re
import json

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
    elif os_distro == 'ubuntu':
        pkg_directory = path.join(pmdk_path, 'dpkg')

    version = ''
    architecture = ''
    for elem in listdir(pkg_directory):
        if os_distro != 'ubuntu':
            if '.src.rpm' in elem:
                # looks for the version number of package in package name
                version = re.search(r'[\s]*pmdk-([\S]+).src.rpm', elem).group(1)
            else:
                architecture = elem

        elif os_distro == 'ubuntu':
            if '.changes' in elem:
                # looks for the version number of packages in package name
                version = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(1)
                architecture = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(2)
    return version, architecture


def save_pkg_version(pkg_version_file_path):
    values_to_save = {
        'PMDK_VERSION': PMDK_VERSION,
        'SYSTEM_ARCHITECTURE': SYSTEM_ARCHITECTURE
        }
    with open(pkg_version_file_path, 'w') as file:
        json.dump(values_to_save, file, indent=2)


def get_built_rpm_packages(pmdk_path):
    """
    Returns built pkg packages from pkg directory.
    """
    path_to_rpm_files = path.join(pmdk_path, 'rpm', SYSTEM_ARCHITECTURE)
    packages = listdir(path_to_rpm_files)
    return packages


def get_built_dpkg_packages(pmdk_path):
    """
    Returns built pkg packages from pkg directory.
    """
    path_to_dpkg_files = path.join(pmdk_path, 'dpkg')
    packages =''
    for elem in listdir(path_to_dpkg_files):
        if '.deb' in elem:
            packages += elem + ' '

    return packages


def install_rpm_packages(pmdk_path):
    """
    Install packages from PMDK library.
    """
    packages = ' '.join(get_built_rpm_packages(pmdk_path))
    path_to_rpm_files = path.join(pmdk_path, 'rpm', SYSTEM_ARCHITECTURE)
    check_output('rpm -i --nodeps ' + packages,
                 cwd=path_to_rpm_files, shell=True)


def install_dpkg_packages(pmdk_path):
    """
    Install packages from PMDK library.
    """
    packages = get_built_dpkg_packages(pmdk_path)
    path_to_dpkg_files = path.join(pmdk_path, 'dpkg')
    check_output('dpkg -i ' + packages,
                 cwd=path_to_dpkg_files, shell=True)


def get_names_of_rpm_content():
    """
    Returns names of elements, for which are installed from packages from PMDK
    library.
    """
    packages_path = path.join(args.pmdk_path, 'rpm', SYSTEM_ARCHITECTURE)
    installed_packages = check_output(
        'ls | grep ' + PMDK_VERSION, cwd=packages_path, shell=True)
    delimiter  = '-'

    installed_packages = installed_packages.decode(
        'UTF-8').split(linesep)

    libraries_names = [item.split(
        '-')[0] for item in installed_packages if item.split('-')[0]]
    return set(libraries_names)


def get_names_of_dpkg_content():
    """
    Returns names of elements, for which are installed from packages from PMDK
    library.
    """
    packages_path = path.join(args.pmdk_path, 'dpkg')
    installed_packages = check_output(
        'ls | grep ' + PMDK_VERSION + ' | grep .deb', cwd=packages_path, shell=True)

    installed_packages = installed_packages.decode(
        'UTF-8').split(linesep)

    libraries_names = [item.split(
        '_')[0] for item in installed_packages if item.split('-')[0]]
    return set(libraries_names)


def get_installed_packages(so_path, split_param, packages_path):
    """
    Returns names of packages from PMDK library, which are installed.
    """
    libraries = get_names_of_pkg_content_func()
    installed_packages = []

    for library in libraries:
        if library == "pmempool" and check_output(
                'find /usr/bin/ -name ' + library, cwd=args.pmdk_path,
                shell=True):
            installed_packages.append(library)
        elif library == "libpmemobj++" and check_output(
                'find /usr/include/' + library + ' -name *.hpp',
                cwd=args.pmdk_path, shell=True):
            installed_packages.append(library)
        elif library == "pmdk":
            pass
        elif check_output('find ' + so_path + ' -name ' + library + '.so',
                          cwd=args.pmdk_path, shell=True):
            installed_packages.append(library)
    return installed_packages


if __name__ == '__main__':
    parser = ArgumentParser(
        description='Install packages from PMDK library')
    parser.add_argument("-r", "--pmdk-path", required=True,
                        help="the PMDK library root path.")

    args = parser.parse_args()
    os_distro=distro.id()
    PMDK_VERSION, SYSTEM_ARCHITECTURE =\
        get_package_version_and_system_architecture(args.pmdk_path)
    save_pkg_version(args.pmdk_path + "/pkgVersion.json")
    if os_distro != 'ubuntu':
        so_path = '/usr/lib64/'
        split_param = '-'
        packages_path = path.join(args.pmdk_path, 'rpm', SYSTEM_ARCHITECTURE)
        install_cmd = 'rpm -i --nodeps '
        install_func = install_rpm_packages
        get_names_of_pkg_content_func = get_names_of_rpm_content
    elif os_distro == 'ubuntu':
        so_path = '/lib/x86_64-linux-gnu/'
        split_param = '_'
        packages_path = path.join(args.pmdk_path, 'dpkg')
        install_cmd = 'dpkg -i '
        install_func = install_dpkg_packages
        get_names_of_pkg_content_func = get_names_of_dpkg_content

    if not get_installed_packages(so_path, packages_path, get_names_of_pkg_content_func):
        install_func(args.pmdk_path)
    else:
        print("PMDK library is still installed")
