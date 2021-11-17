# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

from ctypes import cdll, c_char_p, c_int
from loggingfacility import LoggingBase
from os import path, system
from sys import exit

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

    def __init__(self, library_name, func_name, logger=None):
        """
        Loads the consistency checking function from the given library.

        :param library_name: The full name of the library.
        :type library_name: str
        :param func_name: The name of the consistency
                          checking function within the library.
        :type func_name: str
        :param logger: logger handle, default: empty logger (LoggingBase)
        :type logger: subclass of :class:`LoggingBase`
        :return: None
        """
        self._lib_name = library_name
        self._lib_func_name = func_name
        self._lib_func = getattr(cdll.LoadLibrary(library_name), func_name)
        self._lib_func.argtypes = [c_char_p]
        self._lib_func.restype = c_int
        self._logger = logger or LoggingBase()

    def check_consistency(self, filename):
        """
        Checks the consistency of a given file
        using the previously loaded function.

        :param filename: The full name of the file to be checked.
        :type filename: str
        :return: 1 if file is consistent, 0 otherwise.
        :rtype: int
        :raises: Generic exception, when no function has been loaded.
        """
        if self._lib_func is None:
            raise RuntimeError(
                "Consistency check function {} not loaded".format(
                    self._lib_func_name
                )
            )

        self._logger.debug(
            "Consistency check function: {0}.{1}({2})".format(
                self._lib_name, self._lib_func_name, filename
            )
        )
        return self._lib_func(filename)


class ProgChecker(ConsistencyCheckerBase):
    """
    Allows registration of a consistency checking program and verifying
    the consistency of a file.
    """

    def __init__(self, bin_path, bin_args, logger):
        self._bin_path = bin_path
        self._bin_cmd = bin_args
        self._logger = logger

    def check_consistency(self, filename):
        """
        Checks the consistency of a given file
        using the previously loaded function.

        :param filename: The full name of the file to be checked.
        :type filename: str
        :return: 1 if file is consistent, 0 otherwise.
        :rtype: int
        :raises: Generic exception, when no function has been loaded.
        """
        if self._bin_path is None or self._bin_cmd is None:
            raise RuntimeError("consistency check handle not set")

        cmd = "{0} {1} {2}".format(self._bin_path, self._bin_cmd, filename)
        self._logger.debug("Consistency check program command: {}".format(cmd))
        return system(cmd)


def get_checker(checker_type, checker_path_args, name, logger):

    checker_path_args = checker_path_args.split(" ", 1)
    checker_path = checker_path_args[0]

    # check for params
    if len(checker_path_args) > 1:
        args = checker_path_args[1]
    else:
        args = ""

    if not path.exists(checker_path):
        print("Invalid path:" + checker_path)
        exit(1)

    checker = None
    if checker_type == "prog":
        checker = ProgChecker(checker_path, args, logger)
    elif checker_type == "lib":
        checker = LibChecker(checker_path, name, logger)

    return checker
