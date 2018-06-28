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

from operationfactory import OperationFactory
from binaryoutputhandler import BinaryOutputHandler
import reorderengines


class OpsContext:
    """
    Holds the context of the performed operations.

    :ivar _operations: The operations to be performed, based on the log file.
    :type _operations: list of strings
    :ivar reorder_engine: The reordering engine used at the moment.
    :ivar test_on_barrier: Check consistency on barrier.
    :type test_on_barrier: bool
    :ivar file_handler: The file handler used.
    """
    def __init__(self, log_file, checker, logger):
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
        self.reorder_engine = reorderengines.FullReorderEngine()
        self.test_on_barrier = True
        self.file_handler = BinaryOutputHandler(checker)
        self.checker = checker
        self.logger = logger

    # TODO this should probably be made a generator
    def extract_operations(self):
        """
        Creates specific operation objects based on the labels available
        in the split log file.

        :return: list of subclasses of :class:`memoryoperations.BaseOperation`
        """
        stop_index = start_index = 0

        for i, elem in enumerate(self._operations):
            if "START" in elem:
                start_index = i
            elif "STOP" in elem:
                stop_index = i

        return list(map(OperationFactory.create_operation,
                        self._operations[start_index + 1:stop_index]))
