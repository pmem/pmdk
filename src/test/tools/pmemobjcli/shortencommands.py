#!/usr/bin/env python3
#
# Copyright 2018, Intel Corporation
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
import re
import os.path

pocli_structure_name = 'pocli_cmd pocli_commands'

parser = argparse.ArgumentParser(description='Script used to shorten commands found int pmemobjcli.c.')
parser.add_argument('-s', '--source', type=argparse.FileType('r', encoding='UTF-8'), required=True,
                    help='Source file containing the pmemobjcli commands')
parser.add_argument('-i', '--input', type=argparse.FileType('r', encoding='UTF-8'), required=True, help='Input file')
parser.add_argument('-o', '--output', type=argparse.FileType('w', encoding='UTF-8'), help='Output file',
                    default='output')
args = parser.parse_args()


# returns the block of code where the pocli commands start
def pmem_obj_cli_functions_block(pmem_obj_cli_file):
    lines = []

    start_appending = False
    start_of_command_block = -1
    end_of_command_block = -1
    for i, line in enumerate(open(pmem_obj_cli_file)):
        if pocli_structure_name in line:
            start_of_command_block = i
            start_appending = True
        if start_appending:
            lines.append(line)
            if '};' in line:
                end_of_command_block = i
        if start_of_command_block > 0 and end_of_command_block > 0:
            break
    return lines


# returns a dictionary of pocli commands where the name is the key, and short name is the value
def shortened_command_names(lines):
    dictionary = {}
    for i, line in enumerate(lines):
        if pocli_structure_name not in line and ('{' in line):
            long_name = lines[i+1].strip()
            # remove quotation marks and comments from long name
            long_name = re.sub("\",.*", "", long_name)
            long_name = long_name.replace("\"", "")

            short_name = lines[i + 2].strip()
            # remove quotation marks and comments from short name
            short_name = re.sub("\",.*", "", short_name)
            short_name = short_name.replace("\"", "")

            dictionary[long_name] = short_name

    return dictionary


# returns lines from the input file after shortening the pocli commands
def parse_input_file(input_file, dictionary):
    lines = []
    for i, line in enumerate(open(input_file)):
        split_line = line.split()
        if len(split_line) > 0:
            command = split_line[0]
            if command in dictionary:
                line = line.replace(command, dictionary[command])
        lines.append(line)
    return lines


def write_lines_to_output(lines, output_file_path):
    output_file = open(output_file_path, 'w')
    output_file.writelines(lines)
    output_file.close()
    print("Output written to : " + os.path.abspath(output_file.name))


def main():
    print("Path to pmemobj-cli.c file: %s" % args.source.name)
    lines = pmem_obj_cli_functions_block(args.source.name)
    shortened_commands_dictionary = shortened_command_names(lines)
    print("Path to input file: " + args.input.name)
    output = parse_input_file(args.input.name, shortened_commands_dictionary)
    write_lines_to_output(output, args.output.name)


if __name__ == '__main__':
    main()
