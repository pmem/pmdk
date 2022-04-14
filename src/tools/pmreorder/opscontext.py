# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2022, Intel Corporation

from operationfactory import OperationFactory
from binaryoutputhandler import BinaryOutputHandler
import reorderengines
import memoryoperations
from itertools import repeat


class OpsContext:
    """
    Holds the context of the performed operations.

    :ivar _operations: The operations to be performed, based on the log file.
    :type _operations: list of strings
    :ivar reorder_engine: The reordering engine used at the moment.
    :type one of the reorderengine Class
    :ivar default_engine: The default reordering engine.
    :type default_engine: One of the reorderengines Class
    :ivar test_on_barrier: Check consistency on barrier.
    :type test_on_barrier: bool
    :ivar default_barrier: Default consistency barrier status.
    :type default_barrier: bool
    :ivar file_handler: The file handler used.
    """

    def __init__(self, log_file, checker, logger, arg_engine, markers):
        """
        Splits the operations in the log file and sets the instance variables
        to default values.

        :param log_file: The full name of the log file.
        :type log_file: str
        :return: None
        """
        # TODO reading the whole file at once is rather naive
        # change in the future
        self._operations = open(log_file).read().split("|")
        engine = reorderengines.get_engine(arg_engine)
        self.reorder_engine = engine
        self.test_on_barrier = engine.test_on_barrier
        self.default_engine = self.reorder_engine
        self.default_barrier = self.default_engine.test_on_barrier
        self.file_handler = BinaryOutputHandler(checker, logger)
        self.checker = checker
        self.logger = logger
        self.markers = markers
        self.stack_engines = [("START", getattr(memoryoperations, arg_engine))]

    # TODO this should probably be made a generator
    def extract_operations(self):
        """
        Creates specific operation objects based on the labels available
        in the split log file.

        :return: list of subclasses of :class:`memoryoperations.BaseOperation`
        """
        enumerated_ops = list(enumerate(self._operations))
        markers = list(
            filter(
                lambda e: e[1].endswith(".BEGIN") or e[1].endswith(".END"),
                enumerated_ops,
            )
        )
        operation_ids = list(enumerated_ops)

        stop_index = start_index = 0

        for i, elem in enumerated_ops:
            if "START" in elem:
                start_index = i
            elif "STOP" in elem:
                stop_index = i

        operations = list(
            map(
                OperationFactory.create_operation,
                self._operations[start_index + 1: stop_index],
                repeat(self.markers),
                repeat(self.stack_engines),
            )
        )

        return (operations, operation_ids, markers)
