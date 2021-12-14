#!/usr/bin/env python3
#
# Copyright 2019-2020, Intel Corporation
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
#


from processors import process
import sys
from os import path

# default output file path:
output_file_path = "JUnit.xml"

# non-zero exit code:
failure_status = 1


def help_string():
    string = r'''
    NAME
        {scriptname} - convert PMDK tests console output to JUnit XML report format.
                  
    SYNOPSIS
        python3 {scriptname} INPUT [OUTPUT]
        python3 {scriptname} --help
                  
    DESCRIPTION
        Convert console output from PMDK tests, read from the INPUT file, to the JUnit XML report
        format and save the result to OUTPUT file.
        
        --help
            Print help message.
        
        INPUT
            Valid path to the file with console output from PMDK tests. This argument is mandatory.
            
        OUTPUT
            Path to the file where the result should be written. This argument is optional.
            When not specified, output file will be saved to script's local directory with name {default_name}'''\
        .format(scriptname=path.basename(__file__), default_name=output_file_path)

    return string


# check if correct number of args passed:
if len(sys.argv) in [2, 3]:

    # print help if requested:
    if sys.argv[1] == "--help":
        print(help_string())
        sys.exit()

    # if specified output file path, overwrite default:
    if len(sys.argv) == 3:
        output_file_path = sys.argv[2]

        # check if it's correct path:
        try:
            f = open(output_file_path, 'w')
            f.close()
        except OSError:
            print("Output file name is invalid:", output_file_path, "\n")
            print(help_string())
            sys.exit(failure_status)

    input_file_path = sys.argv[1]

    # if input file exists, do the conversion:
    if path.exists(input_file_path) and path.isfile(input_file_path):
        with open(input_file_path, 'r', encoding='utf-8') as input_file:
            input_string = input_file.read()

        output_string = process.process(input_string)

        with open(output_file_path, 'w', encoding='utf-8') as output_file:
            output_file.write(output_string)
    else:
        print("There is no such file:", input_file_path, "\n")
        print(help_string())
        sys.exit(failure_status)
else:
    print("Invalid arguments' number:", len(sys.argv) - 1, "\n")
    print(help_string())
    sys.exit(failure_status)
