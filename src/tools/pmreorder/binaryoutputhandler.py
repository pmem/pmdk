# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

from loggingfacility import LoggingBase
from reorderexceptions import InconsistentFileException
from sys import byteorder
import utils
import os


class BinaryOutputHandler:
    """
    Handle :class:`BinaryFile` objects.

    Creates and aggregates :class:`BinaryFile` objects for ease of use.
    Implements methods for batch handling of aggregated files.

    :ivar _files: A list of registered files, most recent last.
    :type _files: list
    """

    def __init__(self, checker, logger=None):
        """
        Binary handler constructor.

        :param checker: consistency checker object
        :type checker: ConsistencyCheckerBase
        :param logger: logger handle, default: empty logger (LoggingBase)
        :type logger: subclass of :class:`LoggingBase`
        """
        self._files = []
        self._checker = checker
        self._logger = logger or LoggingBase()

    def add_file(self, file, map_base, size):
        """
        Create and append a mapped file to :attr:`_files`.

        :param file: Full path of the mapped file to be added.
        :type file: str
        :param map_base: Base address of the mapped file.
        :type map_base: int
        :param size: Size of the file.
        :type size: int
        :return: None
        """
        self._files.append(
            BinaryFile(file, map_base, size, self._checker, self._logger)
        )

    def remove_file(self, file):
        """Remove file from :attr:`_files`.

        :param file: File to be removed.
        :type file: str
        :return: None
        """
        for bf in self._files:
            if bf.file_name is file:
                self._files.remove(bf)

    def do_store(self, store_op):
        """
        Perform a store to the given file.

        The file is chosen based on the address and size
        of the store.

        :param store_op: The store operation to be performed.
        :type store_op: Store
        :return: None
        :raises: Generic exception - to be precised later.
        """
        store_ok = False
        for i, bf in enumerate(self._files):
            if utils.range_cmp(store_op, bf) == 0:
                self._logger.debug(
                    "Doing store in file no. {0}: {1}".format(i, bf)
                )
                bf.do_store(store_op)
                store_ok = True
        if not store_ok:
            raise OSError(
                "No suitable file found for store {}".format(store_op)
            )

    def do_revert(self, store_op):
        """
        Reverts a store made to a file.

        Performing a revert on a store that has not been made
        previously yields undefined behavior.

        :param store_op: The store to be reverted.
        :type store_op: Store
        :return: None
        :raises: Generic exception - to be precised later.
        """
        revert_ok = False
        for bf in self._files:
            if utils.range_cmp(store_op, bf) == 0:
                bf.do_revert(store_op)
                revert_ok = True
        if not revert_ok:
            raise OSError(
                "No suitable file found for store {}".format(store_op)
            )

    def check_consistency(self):
        """
        Checks consistency of each registered file.

        :return: None
        :raises: Generic exception - to be precised later.
        """
        for bf in self._files:
            if not bf.check_consistency():
                raise InconsistentFileException(
                    "File {} inconsistent".format(bf)
                )


class BinaryFile(utils.Rangeable):
    """Binary file handler.

    It is a handler for binary file operations. Internally it
    uses mmap to write to and read from the file.

    :ivar _file_name: Full path of the mapped file.
    :type _file_name: str
    :ivar _map_base: Base address of the mapped file.
    :type _map_base: int
    :ivar _map_max: Max address of the mapped file.
    :type _map_max: int
    :ivar _file_map: Memory mapped from the file.
    :type _file_map: mmap.mmap
    :ivar _checker: consistency checker object
    :type _checker: ConsistencyCheckerBase
    """

    def __init__(self, file_name, map_base, size, checker, logger=None):
        """
        Initializes the binary file handler.

        :param file_name: Full path of the mapped file to be added.
        :type file_name: str
        :param map_base: Base address of the mapped file.
        :type map_base: int
        :param size: Size of the file.
        :type size: int
        :param checker: consistency checker object
        :type checker: ConsistencyCheckerBase
        :param logger: logger handle, default: empty logger (LoggingBase)
        :type logger: subclass of :class:`LoggingBase`
        :return: None
        """
        self._file_name = file_name
        self._map_base = map_base
        self._map_max = map_base + size
        # TODO consider mmaping only necessary parts on demand
        self._file_map = utils.memory_map(file_name)
        self._checker = checker
        self._logger = logger or LoggingBase()

    def __str__(self):
        return "{0} (base: {1}, size: {2})".format(
            self._file_name,
            hex(self._map_base),
            hex(self._map_max - self._map_base)
        )

    def do_store(self, store_op):
        """
        Perform the store on the file.

        The store records the old value for reverting.

        :param store_op: The store to be performed.
        :type store_op: Store
        :return: None
        """
        base_off = store_op.get_base_address() - self._map_base
        max_off = store_op.get_max_address() - self._map_base
        # read and save old value
        store_op.old_value = bytes(self._file_map[base_off:max_off])
        self._logger.debug(
            "do_store: old_value: {0}, new_value: {1}".format(
                hex(int.from_bytes(store_op.old_value, byteorder=byteorder)),
                hex(int.from_bytes(store_op.new_value, byteorder=byteorder))
            )
        )

        # write out the new value
        pagesize = os.sysconf("SC_PAGE_SIZE")
        self._file_map[base_off:max_off] = store_op.new_value
        self._file_map.flush(base_off & ~(pagesize - 1), pagesize)

    def do_revert(self, store_op):
        """
        Reverts the store.

        Write back the old value recorded while doing the store.
        Reverting a store which has not been made previously has
        undefined behavior.

        :param store_op: The store to be reverted.
        :type store_op: Store
        :return: None
        """
        base_off = store_op.get_base_address() - self._map_base
        max_off = store_op.get_max_address() - self._map_base

        self._logger.debug(
            "do_revert: old_value: {}".format(
                hex(int.from_bytes(store_op.old_value, byteorder=byteorder))
            )
        )
        # write out the old value
        pagesize = os.sysconf("SC_PAGE_SIZE")
        self._file_map[base_off:max_off] = store_op.old_value
        self._file_map.flush(base_off & ~(pagesize - 1), pagesize)

    def check_consistency(self):
        """
        Check consistency of the file.

        :return: True if consistent, False otherwise.
        :rtype: bool
        """
        return self._checker.check_consistency(self._file_name) == 0

    def get_base_address(self):
        """
        Returns the base address of the file.

        Overrides from :class:`utils.Rangeable`.

        :return: The base address of the mapping passed to the constructor.
        :rtype: int
        """
        return self._map_base

    def get_max_address(self):
        """
        Get max address of the file mapping.

        Overrides from :class:`utils.Rangeable`.

        :return: The max address of the mapping.
        :rtype: int
        """
        return self._map_max
