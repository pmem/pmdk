#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2023, Intel Corporation

"""
This module includes functions to check if documentation of PMDK library
is built correctly and tests which use these functions.

Tests check:
-if all macros from header files are included in the doc directory,
-if all functions from .so files are included in the doc directory,
-if all functions and macros from the doc directory, are included
 in headers files and .so files,
-if all functions and macros have direct access to man page through use
 command 'man function_name/macro_name'.

Required argument:
  -r <PMDK_path>            the PMDK library root path.
  -i <PMDK_install_path>    PMDK library installation path.
"""

import unittest
from subprocess import check_output, call, DEVNULL
from os import listdir, path, linesep, walk
import re
import sys

# definitions of regexes
EXPRESSION_AT_THE_LINE_START = r'[\s]*([a-zA-Z_]+)[\\(\s]+'
EXPRESSION_AFTER_DEFINE_PHRASE = r'[\s]*#define[\s]*([a-zA-Z_]+)[\\(\s]+'

PMDK_LIBRARIES = ['libpmem', 'libpmemblk', 'libpmemlog', 'libpmemobj',
                  'libpmempool', 'libpmem2']


def get_exceptions(pmdk_path):
    """
    Returns exceptions in a list. File 'exceptions.txt' contains all macros,
    which do not have a description in a man page by design.
    """
    exceptions_file_path =\
        path.join(path.dirname(path.realpath(__file__)), 'exceptions.txt')
    with open(exceptions_file_path, 'r') as f:
        exceptions = [line.strip().lower() for line in f]
    return exceptions


def parse_macro_name(exceptions, line, const_expression, macros):
    """
    Parses the name of the macro and adds it to existing list.
    """
    macro_name = re.search(const_expression, line).group(1).lower()
    if macro_name not in exceptions:
        macros.append(macro_name)


def get_macros(pmdk_path):
    """
    Parses all headers from the libpmemobj library, extracts all macros, and
    returns them in a list.
    """
    macros = []
    exceptions = get_exceptions(pmdk_path)
    is_macro_in_next_line = False
    includes_path = path.join(pmdk_path, 'src', 'include', 'libpmemobj')
    # Definition of macro occurs after the phrases: 'static inline' or '#define'
    for header_file in listdir(includes_path):
        with open(path.join(includes_path, header_file), 'r') as f:
            file_lines = f.readlines()
        for line in file_lines:
            if line.startswith('static inline'):
                is_macro_in_next_line = True
            elif is_macro_in_next_line:
                parse_macro_name(exceptions, line,
                                 EXPRESSION_AT_THE_LINE_START, macros)
                is_macro_in_next_line = False
            elif '#define' in line:
                parse_macro_name(exceptions, line,
                                 EXPRESSION_AFTER_DEFINE_PHRASE, macros)
    return macros


def get_functions_from_so_files(lib_path):
    """
    Returns names of functions in a list based on symbols 'T' from .so files.
    """
    functions_from_so_files = []
    path_to_so_files = lib_path
    for elem in listdir(path_to_so_files):
        if elem.endswith('.so') and elem.split('.')[0] in PMDK_LIBRARIES:
            process = check_output(
                'nm ' + elem + ' | grep " T "', cwd=path_to_so_files,
                shell=True)
            out = process.decode('UTF-8')
            for line in out.split(linesep):
                if line:
                    name = line.split(' ')[2].strip()
                    # Exclude'_pobj_debug_notice', because it is a name of
                    # a function which is not described in the man page
                    # by design.
                    if name != '_pobj_debug_notice':
                        functions_from_so_files.append(name)
    return functions_from_so_files


def get_functions_windows_only():
    """
    Returns list of PMDK functions that are specific to Windows OS
    """
    return ['pmem2_source_from_handle', 'pmem2_source_get_handle']


def get_functions_and_macros_from_doc(pmdk_path):
    """
    Returns names of functions and macros in a list based on names of files
    from the doc directory.
    """
    path_to_functions_and_macros = path.join(pmdk_path, 'doc')
    functions_and_macros_from_doc = []
    for _, _, files in walk(path_to_functions_and_macros):
        for f in files:
            if not f.endswith('.3'):
                continue
            # Files with extension '.3' have the same name as functions and
            # macros of PMDK library. 'pmemobj_action' is excluded, because
            # it is not a name of the function.
            if f.startswith('pmemobj_action'):
                continue
            functions_and_macros_from_doc.append(f.split('.')[0])
    return functions_and_macros_from_doc


def check_completeness_of_macros_in_doc(pmdk_path):
    """
    Returns missing macros in the doc directory in a list, based on macros
    extracted from header files.
    """
    macros = get_macros(pmdk_path)
    functions_from_doc = get_functions_and_macros_from_doc(pmdk_path)
    missing_macros = [macro for macro in macros if macro not in
                      functions_from_doc]
    return missing_macros


def check_completeness_of_functions_in_doc(pmdk_path, lib_path):
    """
    Returns list of missing macros in the doc directory, based on macros
    extracted from .so files.
    """
    functions_from_so_files = get_functions_from_so_files(lib_path)
    functions_from_doc = get_functions_and_macros_from_doc(pmdk_path)
    missing_functions_in_doc = [function for function in functions_from_so_files
                                if function not in functions_from_doc]
    return missing_functions_in_doc


def check_completeness_of_extracted_functions_and_macros(pmdk_path, lib_path):
    """
    Returns list of missing macros in header files and missing functions in .so
    files, based on functions and macros from the doc directory.
    """
    functions_from_doc = get_functions_and_macros_from_doc(pmdk_path)
    macros = get_macros(pmdk_path)
    functions_from_so_files = get_functions_from_so_files(lib_path)
    functions_windows_specific = get_functions_windows_only()
    pmem2_async_group_name = 'pmem2_async'
    missing_functions_and_macros_in_doc = [
        item for item in functions_from_doc
        if (item not in macros and item not in functions_from_so_files and
            item not in functions_windows_specific and item not in pmem2_async_group_name)]
    return missing_functions_and_macros_in_doc


def check_linking_manpages(pmdk_path, doc_path):
    """
    Checks if macros and functions from the doc directory have
    direct access to man page through command 'man function_name/macro_name'
    and returns list with elements which are not linked with the man page.
    """
    functions_from_doc = get_functions_and_macros_from_doc(pmdk_path)
    functions_without_manpage = []
    for function in functions_from_doc:
        if doc_path:
            process_find_in_manpage = call(
                'man -M ' + doc_path + ' ' + function, shell=True,
                stdout=DEVNULL, stderr=DEVNULL)
        else:
            process_find_in_manpage = call(
                'man ' + function, shell=True, stdout=DEVNULL, stderr=DEVNULL)
        if process_find_in_manpage:
            functions_without_manpage.append(function)
    return functions_without_manpage


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


class TestDocumentation(unittest.TestCase):

    def test_completeness_of_macros_in_doc(self):
        """
        Checks if all extracted macros from header files are in the doc
        directory.
        """
        missing_macros = check_completeness_of_macros_in_doc(pmdk_path)
        error_msg = linesep + 'List of missing macros in the doc directory:'
        for macro in missing_macros:
            error_msg += linesep + macro
        self.assertFalse(missing_macros, error_msg)

    def test_completeness_of_functions_in_doc(self):
        """
        Checks if all extracted functions from .so files are in the doc
        directory.
        """
        missing_functions =\
            check_completeness_of_functions_in_doc(pmdk_path, lib_path)
        error_msg = linesep + 'List of missing functions in the doc directory:'
        for function in missing_functions:
            error_msg += linesep + function
        self.assertFalse(missing_functions, error_msg)

    def test_completeness_of_extracted_functions_and_macros(self):
        """
        Checks if all functions and macros from the doc directory,
        are extracted from header files and .so files.
        """
        missing_functions_and_macros =\
            check_completeness_of_extracted_functions_and_macros(
                pmdk_path, lib_path)
        error_msg = linesep + 'List of missing macros and functions:'
        for elem in missing_functions_and_macros:
            error_msg += linesep + elem
        self.assertFalse(missing_functions_and_macros, error_msg)

    def test_linking_manpage_for_functions_and_macros(self):
        """
        Checks if all functions and macros have direct access to the man page
        through use command 'man function_name/macro_name'.
        """
        no_linked_functions_and_macros =\
            check_linking_manpages(pmdk_path, doc_path)
        error_msg =\
            linesep + 'List of macros and functions without the linked man\
            page:'
        for elem in no_linked_functions_and_macros:
            error_msg += linesep + elem
        self.assertFalse(no_linked_functions_and_macros, error_msg)


if __name__ == '__main__':
    doc_path = ''
    lib_path = ''
    if '-h' in sys.argv or '--help' in sys.argv:
        print(__doc__)
        unittest.main()
    elif '-r' in sys.argv:
        pmdk_path = parse_argument('-r')
        if '-i' in sys.argv:
            installation_path = parse_argument('-i')
            doc_path = path.join(installation_path, 'share', 'man')
            lib_path = path.join(installation_path, 'lib')
        else:
            print(__doc__)
            print(unittest.main.__doc__)
            exit(1)
        unittest.main()
    else:
        print(__doc__)
        print(unittest.main.__doc__)
