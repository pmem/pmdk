# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018-2021, Intel Corporation

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

    def __str__(self):
        return "Fence"

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
        self.new_value = int(params[2], 16).to_bytes(
            self.size, byteorder=byteorder
        )
        if len(params) > 4:
            self.trace = StackTrace(params[4:])
        else:
            self.trace = StackTrace(
                [
                    "No trace available",
                ]
            )
        self.old_value = None
        self.flushed = False

    def __str__(self):
        return (
            "Store: addr: {0}, size: {1}, val: {2}, stack trace: {3}".format(
                hex(self.address),
                hex(self.size),
                hex(int.from_bytes(self.new_value, byteorder=byteorder)),
                self.trace,
            )
        )

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

    class Factory:
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

    def __str__(self):
        return "Flush: addr: {0} size: {1}".format(
            hex(self._address), hex(self._size)
        )

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

    def __init__(self, values):
        """
        Initializes the object based on the describing string.

        :param values: Pre-formatted string describing values for
            current op.
        :type values: str
        :return: None
        """
        # first value is the op name; for reorder op, it's marker name
        self._marker_name = values.split(";")[0]

    def __str__(self):
        name = self.__class__.__name__
        if self._marker_name is not None:
            name += " -- " + self._marker_name
        return name


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New NoReorderDoCheck object.
            :rtype: NoReorderDoCheck
            """
            return NoReorderDoCheck(values)


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New ReorderFull object.
            :rtype: ReorderFull
            """
            return ReorderFull(values)


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New ReorderAccumulative object.
            :rtype: ReorderAccumulative
            """
            return ReorderAccumulative(values)


class ReorderReverseAccumulative(ReorderBase):
    """
    Describes the type of reordering engine to be used.

    This marker class triggers writing all
    possible reverted accumulative sequences of stores
    between barriers.
    """

    class Factory:
        """
        Internal factory class to be used in dynamic object creation.
        """

        def create(self, values):
            """
            Factory object creation method.

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New ReorderReverseAccumulative object.
            :rtype: ReorderReverseAccumulative
            """
            return ReorderReverseAccumulative(values)


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New NoReorderNoCheck object.
            :rtype: NoReorderNoCheck
            """
            return NoReorderNoCheck(values)


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: ReorderDefault object.
            :rtype: ReorderDefault
            """
            return ReorderDefault(values)


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

            :param values: Pre-formatted string describing values for
                current op.
            :type values: str
            :return: New ReorderPartial object.
            :rtype: ReorderPartial
            """
            return ReorderPartial(values)


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

    def __str__(self):
        return (
            "Register_file: name: {0} addr: {1} size: {2} offset: {3}".format(
                self.name, hex(self.address), hex(self.size), hex(self.offset)
            )
        )

    class Factory:
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
