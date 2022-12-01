# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2022, Intel Corporation
#
"""Tools which allows to easily create poolset files"""


from enum import Enum, unique
import os

import futils
from consts import KiB, MiB, GiB

POOL_MIN_SIZE = 8 * MiB
PART_MIN_SIZE = 2 * MiB


@unique
class CREATE(Enum):
    """
        CREATE allows to create part files during creation of poolset file.
        When ZEROED, HDR_ZEROED, DATA_ZEROED option is passed
        user should specify fsize and optionally mode.

        ZEROED          - create a file filled with \0 characters
        NON_ZEROED       - create a file filled with \132 characters
        HDR_ZEROED      - create a file with a zeroed header and rest filled
                          with \132
    """

    ZEROED = 0
    NON_ZEROED = 1
    HDR_ZEROED = 2


class _Poolset:
    """
    An interface for easy poolset files creation.
    To get new Poolset for testcase call ctx.new_poolset(name).
    Example usage:
    poolset = ctx.new_poolset('poolset')
    poolset.set_parts(File('part0.pool', 10*MiB),
                      DDax(ctx, 'daxpath'))
    poolset.add_replica(File('part0.rep1', 20*MiB),
                        File('part1.rep1', 5*MiB).Create(
                        t.CREATE.ZEROED,  100 * MiB))
    poolset.add_replica(Dir('dirpart.rep1', 2*GiB))
    poolset.set_nohdrs()
    poolset.create()

    """

    def __init__(self, path, ctx):
        self.path = path
        self.parts = []
        self.replicas = []
        self.options = []
        self.ctx = ctx

    def set_parts(self, *parts):
        """
        Adds a main pool to the poolset file.
        The function accepts a list of Parts of any length.
        Should not be called more than once.
        """
        if not self.parts:
            self.parts = list(parts)
        else:
            futils.fail('This function should not be called more than once.')

    def add_replica(self, *parts):
        """
        Adds local replica to poolset file.
        The function accepts a list of Parts of any length.
        Each function call create a new replica.
        """
        self.replicas.append(parts)

    def set_singlehdr(self):
        """
        If called, poolset will be created with SINGLEHDR option.
        """
        self.options.append('SINGLEHDR')

    def set_nohdrs(self):
        """
        If called, poolset will be created with NOHDRS option.
        """
        self.options.append('NOHDRS')

    def create(self):
        """
        Create a poolset file with basic space validation. If CREATE arguments
        ware passed to parts, create parts files as well.
        """
        self._check_pools_size()
        required_size = self._get_required_size()
        free_space = self.ctx.get_free_space()
        if required_size > free_space:
            futils.fail('Not enough space available to create parts '
                        'files. There is {}, and poolset requires {}'
                        .format(free_space, required_size))

        for part in self.parts:
            part._process(self.ctx)
        for replica in self.replicas:
            for part in replica:
                part._process(self.ctx)

        self.options = list(set(self.options))

        poolsetpath = os.path.join(self.ctx.testdir, self.path)
        with open(poolsetpath, 'w') as poolset:
            print('PMEMPOOLSET', end='\n\n', file=poolset)

            for option in self.options:
                print('OPTION', option, end='\n\n', file=poolset)

            for part in self.parts:
                print(part, file=poolset)
            print(file=poolset)

            for replica in self.replicas:
                print("REPLICA", file=poolset)
                for part in replica:
                    print(part, file=poolset)
                print(file=poolset)

    def _check_pools_size(self):
        """"
        Validate if pool and replicas have more than 8MiB (minimum pool size).
        """
        size = 0
        for part in self.parts:
            size += part.size
        if size < POOL_MIN_SIZE:
            futils.fail('The pool has to have at least 8 MiB')
        for replica in self.replicas:
            size = 0
            for part in replica:
                size += part.size
            if size < POOL_MIN_SIZE:
                futils.fail('The pool has to have at least 8 MiB')

    def _get_required_size(self):
        """
        Returns size required to create all part files
        """
        size = 0
        for part in self.parts:
            if part is File and part.create is not None:
                size += part.size
        for replica in self.replicas:
            for part in replica:
                if part is File and part.create is not None:
                    size += part.size
        return size


class _Part:
    """
    An abstraction for poolset parts
    """

    def __init__(self, path, size):
        if size < PART_MIN_SIZE:
            futils.fail('The part should have at least 2 MiB')
        self.size = size
        self.path = path

    def __str__(self):
        return '{} {}'.format(self._format(self.size), self.path)

    def _process(self, ctx):
        """
        Should be overridden by classes whose objects need to create
        files or directories
        """
        pass

    @staticmethod
    def _format(number):
        """
        Formats given number and returns equivalent string with size extension
        """
        if number % GiB == 0:
            return '{}G'.format(int(number / GiB))
        elif number % MiB == 0:
            return '{}M'.format(int(number / MiB))
        elif number % KiB == 0:
            return '{}K'.format(int(number / KiB))
        else:
            return str(number) + 'B'


class File(_Part):
    """
    An interface to file parts creations.
    By default it accepts 2 arguments path and size and does not create a
    passed file. To force file creation use Create(create, fsize, mode=None)
    method.
    Example usage:
    part0 = File('part0', 10 * MiB)
    part1 = File('part1', 20 * MiB).Create(CREATE.ZEROED, 20 * MiB, 0o777)
    part2 = File('part1', 20 * MiB).Create(CREATE.ZEROED, 20 * MiB)
    """

    def __init__(self, path, size):
        _Part.__init__(self, path, size)
        self.create = None

    def Create(self, create, fsize, mode=None):
        self.fsize = fsize
        self.mode = mode
        self.create = create
        return self

    def _process(self, ctx):
        """
        Create file adequately to passed 'create' argument
        """
        if self.create == CREATE.ZEROED:
            ctx.create_holey_file(self.fsize, self.path, self.mode)
        if self.create == CREATE.NON_ZEROED:
            ctx.create_non_zero_file(self.fsize, self.path, self.mode)
        if self.create == CREATE.HDR_ZEROED:
            ctx.create_zeroed_hdr_file(self.fsize, self.path, self.mode)


class DDax(_Part):
    """
    An interface to device dax parts creation
    """

    def __init__(self, ctx, path):
        if not ctx.is_devdax(path):
            futils.fail('Part with path "{}" does not point to dax device'
                        .format(path))
        _Part.__init__(self, path, ctx.get_size(path))


class Dir(_Part):
    """
    An interface to dir parts creation
    """

    def __init__(self, path, size, mode=None):
        _Part.__init__(self, path, size)
        self.mode = mode

    def _process(self, ctx):
        ctx.mkdirs(self.path, self.mode)
