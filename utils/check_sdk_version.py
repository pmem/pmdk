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

import argparse
import os
from subprocess import check_output, CalledProcessError
import sys
import shlex
from xml.dom import minidom
from xml.parsers.expat import ExpatError

VALID_SDK_VERSION = '10.0.16299.0'


def get_vcxproj_files(root_dir, ignored):
    """Get a list ".vcxproj" files under PMDK directory."""
    to_format = []
    command = 'git ls-files *.vcxproj'
    try:
        output = check_output(shlex.split(command),
                              cwd=root_dir).decode("UTF-8")
    except CalledProcessError as e:
        sys.exit('Error: "' + command + '" failed with returncode: ' +
                 str(e.returncode))

    for line in output.splitlines():
        if not line:
            continue
        file_path = os.path.join(root_dir, line)
        if os.path.isfile(file_path):
            to_format.append(file_path)

    return to_format


def get_sdk_version(file):
    """
    Get Windows SDK version from modified/new files from the current
    pull request.
    """
    tag = 'WindowsTargetPlatformVersion'
    try:
        xml_file = minidom.parse(file)
    except ExpatError as e:
        sys.exit('Error: "' + file + '" is incorrect.\n' + str(e))
    version_list = xml_file.getElementsByTagName(tag)
    if len(version_list) != 1:
        sys.exit('Error: the amount of tags "' + tag + '" is other than 1.')
    version = version_list[0].firstChild.data

    return version


def main():
    parser = argparse.ArgumentParser(prog='check_sdk_version.py',
        description='The script checks Windows SDK version in .vcxproj files.')
    parser.add_argument('-d', '--directory',
                        help='Directory of PMDK tree.', required=True)
    args = parser.parse_args()
    current_directory = args.directory
    if not os.path.isdir(current_directory):
        sys.exit('"' + current_directory + '" is not a directory.')

    files = get_vcxproj_files(current_directory, '')
    if not files:
        sys.exit(0)
    for file in files:
        sdk_version = get_sdk_version(file)
        if sdk_version != VALID_SDK_VERSION:
            sys.exit('Wrong Windows SDK version: ' + sdk_version +
                     ' in file: "' + file + '". Please use: ' + VALID_SDK_VERSION)


if __name__ == '__main__':
    main()
