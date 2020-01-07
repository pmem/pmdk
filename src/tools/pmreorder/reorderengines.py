# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018, Intel Corporation


from itertools import combinations
from itertools import permutations
from itertools import islice
from itertools import chain
from random import sample
from functools import partial
from reorderexceptions import NotSupportedOperationException
import collections


class FullReorderEngine:
    def __init__(self):
        self.test_on_barrier = True
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
    def __init__(self):
        self.test_on_barrier = True
    """
    Realizes an accumulative reorder of stores within a given list.
    Example:
        input: (a, b, c)
        output:
               ()
               ('a')
               ('a', 'b')
               ('a', 'b', 'c')
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

        for i in range(0, len(store_list) + 1):
            out_list = [store_list[i] for i in range(0, i)]
            yield out_list


class AccumulativeReverseReorderEngine:
    def __init__(self):
        self.test_on_barrier = True
    """
    Realizes an accumulative reorder of stores
    within a given list in reverse order.
    Example:
        input: (a, b, c)
        output:
               ()
               ('c')
               ('c', 'b')
               ('c', 'b', 'a')
    """
    def generate_sequence(self, store_list):
        """
        Reverse all elements order and
        generates all accumulative lists.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields all accumulative combinations of stores.
        :rtype: iterable
        """
        store_list = list(reversed(store_list))
        for i in range(len(store_list) + 1):
            yield [store_list[j] for j in range(i)]


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
        self.test_on_barrier = True

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
        for sl in islice(chain(*map(lambda x: combinations(store_list, x),
                         range(0, len(store_list) + 1))),
                         self._start, self._stop, self._step):
            yield sl


class FilterPartialReorderEngine:
    """
    Generates a filtered set of the combinations
    without duplication of stores within a given list.
    Example:
        input: (a, b, c), filter = filter_min_elem, kwarg1 = 2
        output:
               (a, b)
               (a, c)
               (b, c)
               (a, b, c)

        input: (a, b, c), filter = filter_max_elem, kwarg1 = 2
        output:
               ()
               (a)
               (b)
               (c)
               (a, b)
               (a, c)
               (b, c)

        input: (a, b, c), filter = filter_between_elem, kwarg1 = 2, kwarg2 = 2
        output:
               (a, b)
               (a, c)
               (b, c)
    """
    def __init__(self, func, **kwargs):
        """
        Initializes the generator with the provided parameters.

        :param func: The filter function.
        :param **kwargs: Arguments to the filter function.
        """
        self._filter = func
        self._filter_kwargs = kwargs
        self.test_on_barrier = True

    @staticmethod
    def filter_min_elem(store_list, **kwargs):
        """
        Filter stores list if number of element is less than kwarg1
        """
        if (len(store_list) < kwargs["kwarg1"]):
            return False
        return True

    @staticmethod
    def filter_max_elem(store_list, **kwargs):
        """
        Filter stores list if number of element is greater than kwarg1.
        """
        if (len(store_list) > kwargs["kwarg1"]):
            return False
        return True

    @staticmethod
    def filter_between_elem(store_list, **kwargs):
        """
        Filter stores list if number of element is
        greater or equal kwarg1 and less or equal kwarg2.
        """
        store_len = len(store_list)
        if (store_len >= kwargs["kwarg1"] and store_len <= kwargs["kwarg2"]):
            return True
        return False

    def generate_sequence(self, store_list):
        """
        This generator yields a filtered set of combinations.

        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields a filtered set of combinations.
        :rtype: iterable
        """
        filter_fun = getattr(self, self._filter, None)
        for elem in filter(
                       partial(filter_fun, **self._filter_kwargs), chain(
                           *map(lambda x: combinations(store_list, x), range(
                               0, len(store_list) + 1)))):
            yield elem


class RandomPartialReorderEngine:
    """
    Generates a random sequence of combinations of stores.
    Example:
        input: (a, b, c), max_seq = 3
        output:
               ('b', 'c')
               ('b',)
               ('a', 'b', 'c')
    """
    def __init__(self, max_seq=3):
        """
        Initializes the generator with the provided parameters.

        :param max_seq: The number of combinations to be generated.
        """
        self.test_on_barrier = True
        self._max_seq = max_seq

    def generate_sequence(self, store_list):
        """
        This generator yields a random sequence of combinations.
        Number of combinations without replacement has to be limited to
        1000 because of exponential growth of elements.
        Example:
            for 10 element from 80 -> 1646492110120 combinations
            for 20 element from 80 -> 3.5353161422122E+18 combinations
            for 40 element from 80 -> 1.0750720873334E+23 combinations
        :param store_list: The list of stores to be reordered.
        :type store_list: list of :class:`memoryoperations.Store`
        :return: Yields a random sequence of combinations.
        :rtype: iterable
        """
        population = list(chain(*map(
                          lambda x: islice(combinations(store_list, x), 1000),
                          range(0, len(store_list) + 1))))
        population_size = len(population)
        for elem in sample(population, self._max_seq if self._max_seq <=
                           population_size else population_size):
            yield elem


class NoReorderEngine:
    def __init__(self):
        self.test_on_barrier = True
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
        :type store_list: The list of :class:`memoryoperations.Store`
        :return: The unmodified list of stores.
        :rtype: iterable
        """
        return [store_list]


class NoCheckerEngine:
    def __init__(self):
        self.test_on_barrier = False
    """
    A NULL reorder engine.
    Example:
        input: (a, b, c)
        output: (a, b, c)
    """
    def generate_sequence(self, store_list):
        """
        This generator does not modify the provided store list
        and does not do the check.

        :param store_list: The list of stores to be reordered.
        :type store_list: The list of :class:`memoryoperations.Store`
        :return: The unmodified list of stores.
        :rtype: iterable
        """
        return [store_list]


def get_engine(engine):
    if engine in engines:
        reorder_engine = engines[engine]()
    else:
        raise NotSupportedOperationException(
                  "Not supported reorder engine: {}"
                  .format(engine))

    return reorder_engine


engines = collections.OrderedDict([
           ('NoReorderNoCheck', NoCheckerEngine),
           ('ReorderFull', FullReorderEngine),
           ('NoReorderDoCheck', NoReorderEngine),
           ('ReorderAccumulative', AccumulativeReorderEngine),
           ('ReorderReverseAccumulative', AccumulativeReverseReorderEngine),
           ('ReorderPartial', RandomPartialReorderEngine)])
