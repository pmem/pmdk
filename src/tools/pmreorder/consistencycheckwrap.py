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

from sys import exit
from os import path
from ctypes import *
import os

checkers = ["prog", "lib"]


class ConsistencyCheckerBase:
    """
    Base class for consistency checker classes.
    Checker of each type should implement check_consistency method.
    """
    def check_consistency(self, filename):
        pass


class LibChecker(ConsistencyCheckerBase):
    """
    Allows registration of a consistency checking function and verifying
    the consistency of a file.

    The function has to be in a shared library. It is then used to check
    consistency of an arbitrary file. The function has to take a file name
    as the only parameter and return an int: 0 for inconsistent, 1 for
    consistent. The prototype of the function::

        int func_name(const char* file_name)
    """

    def __init__(self, library_name, func_name):
        """
        Loads the consistency checking function from the given library.

        :param library_name: The full name of the library.
        :type library_name: str
        :param func_name: The name of the consistency checking function within the library.
        :type func_name: str
        :return: None
        """
        self._lib_func = getattr(cdll.LoadLibrary(library_name), func_name)
        self._lib_func.argtypes = [c_char_p]
        self._lib_func.restype = c_int

    def check_consistency(self, filename):
        """
        Checks the consistency of a given file using the previously loaded function.
        :param filename: The full name of the file to be checked.
        :type filename: str
        :return: 1 if file is consistent, 0 otherwise.
        :rtype: int
        :raises: Generic exception, when no function has been loaded.
        """
        if self._lib_func is None:
            raise RuntimeError("Consistency check function not loaded")
        return self._lib_func(filename)


class ProgChecker(ConsistencyCheckerBase):
    """
    Allows registration of a consistency checking program and verifying
    the consistency of a file.
    """

    def __init__(self, bin_path, bin_args):
        self._bin_path = bin_path
        self._bin_cmd = ' '.join(bin_args)

    def check_consistency(self, filename):
        """
        Checks the consistency of a given file using the previously loaded function.
        :param filename: The full name of the file to be checked.
        :type filename: str
        :return: 1 if file is consistent, 0 otherwise.
        :rtype: int
        :raises: Generic exception, when no function has been loaded.
        """
        if self._bin_path is None or self._bin_cmd is None:
            raise RuntimeError("consistency check handle not set")
        return os.system(self._bin_path + " " + self._bin_cmd + " " + filename)


def get_checker(checker_type, checker_path, name, args):
    if not path.exists(checker_path):
        print("Invalid path:" + checker_path)
        exit(1)

    checker = None
    if checker_type == "prog":
        if args is None:
            args = ""
        checker = ProgChecker(checker_path, args)
    elif checker_type == "lib":
        checker = LibChecker(checker_path, name)

    return checker
