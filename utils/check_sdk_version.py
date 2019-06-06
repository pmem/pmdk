#!/usr/bin/env python3
#
# Copyright 2019, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from sys import argv, exit
from os import path
from subprocess import check_output
from ntpath import basename
from xml.dom import minidom
import argparse

VALID_SDK_VERSION = "10.0.16299.0"


def get_diff_files_to_process(root_dir, ignored):
    """Get a list of changed 'vcxproj' files under root_dir, omit ignored
    path parts."""
    to_format = []
    current_branch = check_output('git rev-parse --abbrev-ref HEAD', shell=True,
                                  cwd=root_dir).decode("UTF-8").strip()
    output = check_output((' ').join(('git diff --name-only --no-renames', current_branch, 'master')), shell=True,
                          cwd=root_dir).decode("UTF-8")
    for line in output.splitlines():
        file_name = basename(line)
        if line and file_name.endswith('.vcxproj'):
            to_format.append(path.join(root_dir, line.replace('/', '\\')))

    return to_format


def get_sdk_version(file):
    """Get SDK version from modified/new files from the current pull request"""
    tag = 'WindowsTargetPlatformVersion'
    xml_file = minidom.parse(file)
    version_list = xml_file.getElementsByTagName(tag)
    if len(version_list) != 1:
        exit("Badly configured 'vcxproj' file")
    version = version_list[0].firstChild.data

    return version


def check_sdk_version(version, file):
    """Check SDK version from modified/new files from the current pull request
    with the valid version"""
    if version != VALID_SDK_VERSION:
        exit("Wrong SDK version: " + version + " in file: " +
             file + ". Please use: " + VALID_SDK_VERSION)


def main():
    files = get_diff_files_to_process(current_directory, '')
    if not files:
        exit(0)
    for file in files:
        if not path.isfile(file):
            continue
        sdk_version = get_sdk_version(file)
        check_sdk_version(sdk_version, file)


if __name__ == '__main__':
    argc = len(argv)
    if argc == 1:
        exit('Usage: check_sdk_version.py -d <pmdk_dir>')

    parser = argparse.ArgumentParser(prog='check_sdk_version.py',
                                     description='The script checks sdk version in .vcxproj files')
    parser.add_argument('-d', '--directory',
                        help='Directory of PMDK library tree.')

    args = parser.parse_args()
    current_directory = args.directory
    if not path.isdir(current_directory):
        exit("Wrong path to PMDK library.")

    main()
