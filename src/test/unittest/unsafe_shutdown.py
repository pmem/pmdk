# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2023, Intel Corporation
#
"""Unsafe shutdown utilities"""

import os
import subprocess as sp
import abc

import futils
import tools


def _get_dev_from_testdir(testdir):
    if not os.path.isdir(testdir):
        raise futils.Fail('{} is not an existing directory'
                          .format(testdir))
    try:
        out = sp.check_output(['df', testdir], stderr=sp.STDOUT,
                              universal_newlines=True)
    except sp.CalledProcessError as e:
        raise futils.Fail('df command failed: {}'.format(e.output)) from e
    else:
        # Assuming that correct 'df <testdir>' output looks as below:
        #
        # Filesystem      1K-blocks  Used Available Use% Mounted on
        # /dev/pmem0     1019003852 77856 967093676   1% /mnt/pmem0
        #
        # the device is the first word in the second line
        try:
            device = out.splitlines()[1].split()[0]
        except IndexError:
            raise futils.Fail('"df {}" output could not be parsed:\n{}'
                              .format(testdir, out))
        if not os.path.exists(device):
            raise futils.Fail('found device "{}" is not an existing file'
                              .format(device))
        return device


class USCTool(abc.ABC):
    """
    An abstract class implementing an interface necessary to serve as an
    unsafe shutdown handler tool for public UnsafeShutdown class.
    """
    @abc.abstractmethod
    def get_dev_dimms(self, dev):
        """
        Get underlying DIMMs of a provided device.
        """
        pass

    @abc.abstractmethod
    def read_usc(self, dimm):
        """
        Read unsafe shutdown count from DIMM.
        It is assumed that DIMM returned from get_dev_dimms
        may serve as a valid 'dimm' argument of this method.
        """
        pass


class UnsafeShutdown:
    """
    Utility class specifying high-level workflow for handling unsafe shutdown.
    "tool" is a class representing specific application used for reading
    and injecting unsafe shutdown. It should be a subclass of USCTool abstract
    class.
    """
    def __init__(self, tool: USCTool = None):
        if tool:
            if not isinstance(tool, USCTool):
                raise futils.Fail('{} should be a subclass of USCTool'
                                  .format(tool))
            self.tool = tool

        # set default injecting tool if no tool provided
        else:
            self.tool = NdctlUSC()

    def read(self, testdir):
        """
        Read unsafe shutdown count of provided device, which is a sum
        of unsafe shutdown counts of all underlying DIMMs
        """
        dev = _get_dev_from_testdir(testdir)
        dimms = self.get_dev_dimms(dev)
        return sum(self.read_from_dimms(*dimms))

    def read_from_dimms(self, *dimms):
        """
        Read unsafe shutdown count values as a list
        """
        return [self.tool.read_usc(d) for d in dimms]

    def get_dev_dimms(self, dev):
        """
        Get the device's underlying DIMMs. For a particular tool their format
        should be valid as an argument input for all methods.
        """
        return self.tool.get_dev_dimms(dev)


class NdctlUSC(tools.Ndctl, USCTool):
    """
    Ndctl with additional unsafe shutdown handling methods
    implementing USCTool abstract class
    """
    def get_dev_dimms(self, dev):
        region = self._get_dev_region(dev)
        return region['mappings']

    def read_usc(self, dimm):
        try:
            name = dimm['dimm']
        except KeyError as e:
            raise futils.Fail(e)

        out = self._cmd_out_to_json('list', '-HD', '-d', name)
        # dimm's health is nested into a single-element array
        usc = int(out[0]['health']['shutdown_count'])

        return usc
