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


import os
import mmap


class Rangeable:
    """
    Interface for all rangeable objects.

    All rangeable objects must be able to return their base and max
    addresses.
    """
    def get_base_address(self):
        """
        Getter for the base address of the object.

        :return: The base address of the object.
        :rtype: int
        """
        raise NotImplementedError

    def get_max_address(self):
        """
        Getter for the max address of the object.

        :return: The max address of the object.
        :rtype: int
        """
        raise NotImplementedError


class StackTrace:
    def __init__(self, trace=None):
        self.trace = trace

    def __str__(self):
        ret = ""
        if self.trace is not None:
            for line in self.trace:
                ret += "    by\t{}\n".format(line)
        return ret


def memory_map(filename, size=0, access=mmap.ACCESS_WRITE, offset=0):
    """
    Memory map a file.

    :Warning:
        `offset` has to be a non-negative multiple of PAGESIZE or
        ALLOCATIONGRANULARITY

    :param filename: The file to be mapped.
    :type filename: str
    :param size: Number of bytes to be mapped. If is equal 0, the
        whole file at the moment of the call will be mapped.
    :type size: int
    :param offset: The offset within the file to be mapped.
    :type offset: int
    :param access: The type of access provided to mmap.
    :return: The mapped file.
    :rtype: mmap.mmap
    """
    fd = os.open(filename, os.O_RDWR)
    m_file =  mmap.mmap(fd, size, access=access, offset=offset)
    os.close(fd)
    return m_file


def range_cmp(lhs, rhs):
    """
    A range compare function.

    :param lhs: The left hand side of the comparison.
    :type lhs: Rangeable
    :param rhs: The right hand side of the comparison.
    :type rhs: Rangeable
    :return: -1 if lhs is before rhs, 1 when after and 0 on overlap.
    :rtype: int

    The comparison function may be explained as::

        Will return -1:
        |___lhs___|
                        |___rhs___|

        Will return +1:
        |___rhs___|
                        |___lhs___|

        Will return 0:
        |___lhs___|
               |___rhs___|
    """
    if lhs.get_max_address() <= rhs.get_base_address():
        return -1
    elif lhs.get_base_address() >= rhs.get_max_address():
        return 1
    else:
        return 0
