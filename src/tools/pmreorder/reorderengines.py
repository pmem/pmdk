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


from itertools import combinations
from itertools import permutations
from itertools import islice
from itertools import chain
from random import sample


class FullReorderEngine:
    """
    Realizes a full reordering of stores within a given list.
    Example:
        input: (a, b, c)
        output:
               ()
               ('a',)
               ('b',)
               ('c',)
               ('a', 'b')
               ('a', 'c')
               ('b', 'a')
               ('b', 'c')
               ('c', 'a')
               ('c', 'b')
               ('a', 'b', 'c')
               ('a', 'c', 'b')
               ('b', 'a', 'c')
               ('b', 'c', 'a')
               ('c', 'a', 'b')
               ('c', 'b', 'a')
    """
    def generate_sequence(self, store_list):
        """
        Generates all possible combinations of all possible lengths,
        based on the operations in the list.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields all combinations of stores.
        :rtype: iterable
        """
        for length in range(0, len(store_list) + 1):
            for permutation in permutations(store_list, length):
                yield permutation


class AccumulativeReorderEngine:
    """
    Example:
        input: (a, b, c)
        output:
               ()
               ('a')
               ('a', 'b')
               ('a', 'b', 'c')
    Realizes an accumulative reorder of stores within a given list.
    """
    def generate_sequence(self, store_list):
        """
        Generates all accumulative lists,
        based on the operations in the store list.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields all accumulative combinations of stores.
        :rtype: iterable
        """

        for i in  range(0, len(store_list) + 1):
            out_list = [ store_list[i] for i in range(0, i) ]
            yield out_list


class SlicePartialReorderEngine:
    """
    Generates a slice of the full reordering of stores within a given list.
    Example:
        input: (a, b, c), start = 2, stop = None, step = 2
        output:
               ('b')
               ('a', 'b')
               ('b', 'c')
    """
    def __init__(self, start, stop, step=1):
        """
        Initializes the generator with the provided parameters.

        :param start: Number of preceding elements to be skipped.
        :param stop: The element at which the slice is to stop.
        :param step: How many values are skipped between successive calls.
        """
        self._start = start
        self._stop = stop
        self._step = step

    def generate_sequence(self, store_list):
        """
        This generator yields a slice of all possible combinations.

        The result may be a set of combinations of different lengths,
        depending on the slice parameters provided at object creation.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields a slice of all combinations of stores.
        :rtype: iterable
        """
        for sl in islice(chain(*map(lambda x: combinations(store_list, x), range(0, len(store_list) + 1))),
                         self._start, self._stop, self._step):
            yield sl


class FilterPartialReorderEngine:
    """
    Generates a filtered set of the full reordering of stores within a given list.
    """
    def __init__(self, filter_):
        """
        Initializes the generator with the provided parameters.

        :param filter_: The filter function.
        """
        self._filter = filter_

    def generate_sequence(self, store_list):
        """
        This generator yields a filtered set of combinations.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields a filtered set of combinations.
        :rtype: iterable
        """
        for elem in filter(chain(*map(lambda x: combinations(store_list, x), range(0, len(store_list) + 1))),
                           self._filter):
            yield elem


class RandomPartialReorderEngine:
    """
    Generates a random sequence of combinations of stores.
    Example:
        input: (a, b, c), max_seq = 2
        output:
               ('b', 'c')
               ('b',)
               ('a', 'b', 'c')
    """
    def __init__(self, max_seq):
        """
        Initializes the generator with the provided parameters.

        :param max_seq: The number of combinations to be generated.
        """
        self._max_seq = max_seq

    def generate_sequence(self, store_list):
        """
        This generator yields a random sequence of combinations.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields a random sequence of combinations.
        :rtype: iterable
        """
        for elem in sample(list(chain(*map(lambda x: combinations(store_list, x), range(0, len(store_list) + 1)))),
                           self._max_seq):
            yield elem


class NoReorderEngine:
    """
    A NULL reorder engine.
    Example:
        input: (a, b, c)
        output: (a, b, c)
    """
    def generate_sequence(self, store_list):
        """
        This generator does not modify the provided store list.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: The unmodified list of stores.
        :rtype: iterable
        """
        return [store_list]
