#!usr/bin/env python3
#
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation

"""
This module includes functions which clean up environment system after
installation of rpm packages from PMDK library.
"""

from os import listdir, path, linesep
import distro
from subprocess import check_output
from argparse import ArgumentParser
import re
import json
from pathlib import Path

PMDK_VERSION = ''
SYSTEM_ARCHITECTURE = ''


def get_package_version_and_system_architecture(pmdk_path):
    """
    Returns packages version and system architecture from names of directories
    from rpm directory.
    """
    version = ''
    architecture = ''
    os_distro=distro.id()

    # first check if there is a json file with version and architecture
    pkg_version_file_path = Path(pmdk_path).joinpath("pkgVersion.json")
    if pkg_version_file_path.exists() and pkg_version_file_path.is_file():
        with open(pkg_version_file_path) as pkg_version_file:
            version_from_file = {}
            try:
                version_from_file = json.load(pkg_version_file)
                version = version_from_file.get('PMDK_VERSION', '')
                architecture = version_from_file.get('SYSTEM_ARCHITECTURE', '')
            except:
                pass

    # if cannot read values from json file, read them from rpms:
    if version == '' or architecture == '':
        if os_distro != 'ubuntu':
            pkg_directory = path.join(pmdk_path, 'rpm')
            for elem in listdir(pkg_directory):
                if '.src.rpm' in elem:
                    version = re.search(r'[\s]*pmdk-([\S]+).src.rpm', elem).group(1)
                else:
                    architecture = elem

        elif os_distro == 'ubuntu':
            pkg_directory = path.join(pmdk_path, 'dpkg')
            for elem in listdir(pkg_directory):
                if '.changes' in elem:
                    # looks for the version number of dpkg package in dpkg package name
                    version = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(1)
                    architecture = re.search(r'pmdk_*(.+)_(.+).changes', elem).group(2)

    return version, architecture


def remove_install_rpm_packages(pmdk_path):
    """
    Removes binaries installed from packages from PMDK library.
    """
    try:
        rpm_to_uninstall = check_output(
            'rpm -qa | grep ' + PMDK_VERSION, cwd=pmdk_path, shell=True)
        pkg_to_uninstall = rpm_to_uninstall.decode(
            'UTF-8').replace(linesep, ' ')
    except:
        pkg_to_uninstall = ''

    if pkg_to_uninstall:
        check_output('rpm -e ' + pkg_to_uninstall, cwd=pmdk_path, shell=True)


def remove_install_dpkg_packages(pmdk_path):
    """
    Removes binaries installed from packages from PMDK library.
    """
    try:
        dpkg_to_uninstall = check_output(
            'dpkg-query --show | grep ' + PMDK_VERSION + ''' | awk '{print $1}' ''', cwd=pmdk_path, shell=True)
        pkg_to_uninstall = dpkg_to_uninstall.decode(
            'UTF-8').replace(linesep, ' ')
    except:
        pkg_to_uninstall = ''

    if pkg_to_uninstall:
        check_output('dpkg -r ' + pkg_to_uninstall, cwd=pmdk_path, shell=True)


if __name__ == '__main__':
    parser = ArgumentParser(
        description='''Clean up system environment after installation rpm
        packages from PMDK library''')
    parser.add_argument("-r", "--pmdk-path", required=True,
                        help="the PMDK library root path.")

    args = parser.parse_args()
    PMDK_VERSION, SYSTEM_ARCHITECTURE =\
        get_package_version_and_system_architecture(args.pmdk_path)

    os_distro=distro.id()
    if os_distro != 'ubuntu':
        remove_install_rpm_packages(args.pmdk_path)
    elif os_distro == 'ubuntu':
        remove_install_dpkg_packages(args.pmdk_path)
