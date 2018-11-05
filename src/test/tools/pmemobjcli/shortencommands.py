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

POCLI_STRUCTURE_NAME = 'pocli_cmd pocli_commands'


def pmemobjcli_functions_block(path):
    """
    Finds the block containing pocli commands in a file with the given path.

    :param path: Path to the pmemobjcli.c file.
    :return: List of lines containing the pocli commands block.
    """
    lines = []
    start_appending = False
    start_of_command_block = -1
    end_of_command_block = -1
    with open(path) as pmemobjcli_file:
        for i, line in enumerate(pmemobjcli_file):
            if POCLI_STRUCTURE_NAME in line:
                start_of_command_block = i
                start_appending = True
            if start_appending:
                lines.append(line)
                if '};' in line:
                    end_of_command_block = i
            if start_of_command_block > 0 and end_of_command_block > 0:
                break
    return lines


def parsed_pocli_commands(lines):
    """
    Creates a dictionary based on the given block of pocli commands.

    Where:

    keys: long names

    values: short names

    :param lines: Lines containing the pocli commands block.
    :type lines: list of str
    :return: Dictionary with the pocli commands.
    """
    commands = {}
    for i, line in enumerate(lines):
        if POCLI_STRUCTURE_NAME not in line and '{' in line:
            if i + 1 < len(lines):
                long_name = lines[i + 1].strip()
                # remove quotation marks and comments from long name
                long_name = re.sub("\",.*", "", long_name)
                long_name = long_name.replace("\"", "")
                if i + 2 < len(lines):
                    short_name = lines[i + 2].strip()
                    # remove quotation marks and comments from short name
                    short_name = re.sub("\",.*", "", short_name)
                    short_name = short_name.replace("\"", "")
                    commands[long_name] = short_name

    return commands


def parse_input_file(input_file_path, commands):
    """
    Parses the file with a specified path based on the given directory.
    Finds every keyword in the dictionary and replaces it with it's value.

    keys: long names

    values: short names

        Example of input file:

        pmemobj_root 1024

        pmemobj_alloc r.0 3 8192

        pmemobj_direct r.0

        pmemobj_alloc_usable_size r.0

        pmemobj_free r.0

        pmemobj_direct r.0


    :param input_file_path: Contains the pocli commands to modify.
    :param commands: The dictionary containing the pocli commands

    :return: List of strings modified based on the dictionary.
    """

    lines = []
    with open(input_file_path) as input_file:
        for i, line in enumerate(input_file):
            split_line = line.split()
            if len(split_line) > 0:
                command = split_line[0]
                if command in commands:
                    line = line.replace(command, commands[command])
                elif '#' not in command:
                    print("Unknown command: {}".format(command))
            lines.append(line)
    return lines


def write_lines_to_output(lines, output_file_path):
    """
    Writes the given lines to an output file with a specified path.

    :param lines:  Lines to be written to output.
    :type lines: list of str
    :param output_file_path: Path to the output file.
    """
    with open(output_file_path, 'w') as output_file:
        output_file.writelines(lines)
    print("Output written to : {}".format(os.path.abspath(output_file.name)))


def main():
    print("Path to pmemobjcli.c file: {}".format(args.source.name))
    lines = pmemobjcli_functions_block(args.source.name)
    shortened_commands = parsed_pocli_commands(lines)
    print("Path to input file: {}".format(args.input.name))
    output = parse_input_file(args.input.name, shortened_commands)
    write_lines_to_output(output, args.output.name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Script shortening the pocli commands found in input file.'
                    'The commands are based on pmemobjcli.c.')
    parser.add_argument('-s', '--source',
                        type=argparse.FileType('r', encoding='UTF-8'),
                        required=True,
                        help='Source file containing the pmemobjcli commands')
    parser.add_argument('-i', '--input',
                        type=argparse.FileType('r', encoding='UTF-8'),
                        required=True,
                        help='Script containing the pmemobjcli commands')
    parser.add_argument('-o', '--output',
                        type=argparse.FileType('w', encoding='UTF-8'),
                        help='Output file', default='shortenedcommands.out')
    args = parser.parse_args()
    main()
