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

from utils import Rangeable
from utils import range_cmp
from utils import StackTrace
from sys import byteorder


class BaseOperation:
    """
    Base class for all memory operations.
    """
    pass


class Fence(BaseOperation):
    """
    Describes a fence operation.

    The exact type of the memory barrier is not important,
    it is interpreted as an SFENCE or MFENCE.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New Fence object.
            :rtype: Fence
            """
            return Fence()


class Store(BaseOperation, Rangeable):
    """
    Describes a store operation.

    :ivar address: The virtual address at which to store the new value.
    :type address: int
    :ivar new_value: The new value to be written.
    :type new_value: bytearray
    :ivar size: The size of the store in bytes.
    :type size: int
    :ivar old_value: The old value read from the file.
    :type old_value: bytearray
    :ivar flushed: Indicates whether the store has been flushed.
    :type flushed: bool
    """
    def __init__(self, values):
        """
        Initializes the object based on the describing string.

        :param values: Pre-formatted string describing the store.
        :type values: str
        :return: None
        """
        params = values.split(";")
        # calculate the offset given the registered file mapping
        self.address = int(params[1], 16)
        self.size = int(params[3], 16)
        self.new_value = \
            int(params[2], 16).to_bytes(self.size, byteorder=byteorder)
        if len(params) > 4:
            self.trace = StackTrace(params[4:])
        else:
            self.trace = StackTrace(["No trace available", ])
        self.old_value = None
        self.flushed = False

    def __str__(self):
        return "addr: " + hex(self.address) + " size " + \
            str(self.size) + " value " + str(self.new_value)

    def get_base_address(self):
        """
        Override from :class:`utils.Rangeable`.

        :return: Virtual address of the store.
        :rtype: int
        """
        return self.address

    def get_max_address(self):
        """
        Override from :class:`utils.Rangeable`.

        :return: Virtual address of the first byte after the store.
        :rtype: int
        """
        return self.address + self.size

    class Factory():
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Pre-formatted string describing the store.
            :type values: str
            :return: New Store object.
            :rtype: Store
            """
            return Store(values)


class FlushBase(BaseOperation, Rangeable):
    """
    Base class for flush operations.
    """
    def is_in_flush(self, store_op):
        """
        Check if a given store is within the flush.

        :param store_op: Store operation to check.
        :return: True if store is in flush, false otherwise.
        :rtype: bool
        """
        raise NotImplementedError


class Flush(FlushBase):
    """
    Describes a flush operation.

    Examples of flush instructions are CLFLUSH, CLFLUSHOPT or CLWB.

    :ivar _address: Virtual address of the flush.
    :type _address: int
    :ivar _size: The size of the flush in bytes (should be cache line aligned).
    :type _size: int
    """
    def __init__(self, values):
        """
        Initializes the object based on the describing string.

        :param values: Pre-formatted string describing the flush.
        :type values: str
        :return: None
        """
        params = values.split(";")
        self._address = int(params[1], 16)
        self._size = int(params[2], 16)

    def is_in_flush(self, store_op):
        """
        Override from :class:`FlushBase`.

        :param store_op: Store operation to check.
        :return: True if store is in flush, false otherwise.
        :rtype: bool
        """
        if range_cmp(store_op, self) == 0:
            return True
        else:
            return False

    def get_base_address(self):
        """
        Override from :class:`utils.Rangeable`.

        :return: Virtual address of the flush.
        :rtype: int
        """
        return self._address

    def get_max_address(self):
        """
        Override from :class:`utils.Rangeable`.

        :return: Virtual address of the first byte after the flush.
        :rtype: int
        """
        return self._address + self._size

    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Pre-formatted string describing the flush.
            :type values: str
            :return: New Flush object.
            :rtype: Flush
            """
            return Flush(values)


class ReorderBase(BaseOperation):
    """
    Base class for all reorder type classes.
    """
    pass


class NoReorderDoCheck(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing the whole sequence of stores
    between barriers.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New NoReorderDoCheck object.
            :rtype: NoReorderDoCheck
            """
            return NoReorderDoCheck()


class ReorderFull(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing all possible sequences of stores
    between barriers.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New ReorderFull object.
            :rtype: ReorderFull
            """
            return ReorderFull()


class ReorderAccumulative(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing all
    possible accumulative sequences of stores
    between barriers.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New ReorderAccumulative object.
            :rtype: ReorderAccumulative
            """
            return ReorderAccumulative()


class NoReorderNoCheck(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing the whole sequence of stores
    between barriers. It additionally marks that no consistency checking
    is to be made.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New NoReorderNoCheck object.
            :rtype: NoReorderNoCheck
            """
            return NoReorderNoCheck()


class ReorderDefault(ReorderBase):
    """
    Describes the default reordering engine to be used.

    This marker class triggers default reordering.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: ReorderDefault object.
            :rtype: ReorderDefault
            """
            return ReorderDefault()


class ReorderPartial(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing a subset of all possible
    sequences of stores between barriers.

    The type of partial reordering is chosen at runtime. Not yet
    implemented.
    """
    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Ignored.
            :type values: str
            :return: New ReorderPartial object.
            :rtype: ReorderPartial
            """
            return ReorderPartial()


class Register_file(BaseOperation):
    """
    Describes the file to be mapped into processes address space.

    :ivar name: The full name of the file.
    :type name: str
    :ivar address: The base address where the file was mapped.
    :type address: int
    :ivar size: The size of the mapping.
    :type size: int
    :ivar offset: The start offset of the mapping within the file.
    :type offset: int
    """
    def __init__(self, values):
        """
        Initializes the object based on the describing string.

        :param values: Pre-formatted string describing the flush.
        :type values: str
        :return: None
        """
        params = values.split(";")
        self.name = params[1]
        self.address = int(params[2], 16)
        self.size = int(params[3], 16)
        self.offset = int(params[4], 16)

    class Factory():
        """
        Internal factory class to be used in dynamic object creation.
        """
        def create(self, values):
            """
            Factory object creation method.

            :param values: Pre-formatted string
                           describing the file registration.
            :type values: str
            :return: New Register_file object.
            :rtype: Register_file
            """
            return Register_file(values)
