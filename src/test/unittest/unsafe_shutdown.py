# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
"""Unsafe shutdown utilities"""

import os
import sys
import shlex
import subprocess as sp

import futils
import tools


if sys.platform == 'win32':
    GET_DISK_NO_CMD = r'powershell (Get-Partition -DriveLetter(Get-Item {}:\)'\
                      '.PSDrive.Name).DiskNumber'

    GET_DIMMS_FROM_DISK_NO_CMD =\
        'powershell Get-PmemDisk | Where DiskNumber '\
        '-Eq {} | Format-Table -Property PhysicalDeviceIds -HideTableHeaders '\
        '-Autosize'

    def get_dimms_from_disk_no(disk_no):
        """
        Get device ID (disk number) form test directory
        using PowerShell cmdlets
        """
        cmd = GET_DIMMS_FROM_DISK_NO_CMD.format(disk_no)
        cmd = shlex.split(cmd)

        proc = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT,
                      universal_newlines=True)
        if proc.returncode:
            futils.Fail('The {} disk number could not be found'
                        .format(proc.stdout))

        return proc.stdout.strip().strip(r'{}').split(',')


def get_dev_from_testdir(testdir):
    if sys.platform == 'win32':
        return _get_dev_from_testdir_win(testdir)
    else:
        return _get_dev_from_testdir_linux(testdir)


def _get_dev_from_testdir_win(testdir):
    volume, _ = os.path.splitdrive(testdir)
    cmd = GET_DISK_NO_CMD.format(volume.rstrip(':'))
    cmd = shlex.split(cmd)
    proc = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT,
                  universal_newlines=True)
    if proc.returncode:
        futils.Fail('The {} disk number could not be found'.format(volume))
    return proc.stdout.strip()


def _get_dev_from_testdir_linux(testdir):
    if not os.path.isdir(testdir):
        raise futils.Fail('{} is not an existing directory'
                          .format(testdir))
    try:
        out = sp.check_output(['mount'], stderr=sp.STDOUT,
                              universal_newlines=True)
    except sp.CalledProcessError as e:
        raise futils.Fail('mount command failed') from e
    else:
        for l in out.splitlines():
            # 'mount' command output line starts with:
            # '<device> on <mountpoint>'
            mountpoint = l.split()[2]
            if mountpoint != '/' and mountpoint in testdir:
                dev = l.split()[0]
                if os.path.exists(dev):
                    return dev
                else:
                    raise futils.Fail('{} is not an existing device'
                                      .format(dev))

        raise futils.Fail('{} provided test directory was not found')


class Ipmctl():
    """ipmctl tool class"""
    def __init__(self):
        if sys.platform != 'win32':
            futils.fail('Ipmctl tool class is currently ipmlemented only'
                        ' for windows - for linux use ndctl instead')

    def get_dev_dimms(self, dev):
        # add hex prefix for compliance with read_usc and inject_usc
        # methods
        dimms = ['0x{}'.format(d) for d in get_dimms_from_disk_no(dev)]
        return dimms

    def read_usc(self, *dimms):
        usc = 0
        raw_cmd = 'ipmctl show -sensor -dimm {}'
        row_title = 'LatchedDirtyShutdownCount'
        for d in dimms:
            out = ''
            cmd = raw_cmd.format(d)
            try:
                out = sp.check_output(cmd.split(), universal_newlines=True,
                                      stderr=sp.STDOUT)
                usc_row = [o for o in out.splitlines() if row_title in o][0]

                # considering 'usc_row' looks like:
                # '0x0101 | LatchedDirtyShutdownCount   | 4           | Normal'
                # 'usc' is the value in the third column:
                usc += int(usc_row.split('|')[2].strip())
            except sp.CalledProcessError as e:
                raise futils.Fail('Reading unsafe shutdown count '
                                  'with ipmctl failed:\n{}'.format(e.output))
            except (IndexError, ValueError) as e:
                raise futils.Fail('Could not read dirty shutdown'
                                  'value from dimm {}. \n{}'
                                  .format(d, e.output))

        return usc

    def inject_usc(self, *dimms):
        cmd_format = 'ipmctl set -dimm {} DirtyShutdown=1'
        for d in dimms:
            try:
                cmd = cmd_format.format(d).split()
                sp.check_output(cmd, universal_newlines=True,
                                stderr=sp.STDOUT)
            except sp.CalledProcessError as e:
                raise futils.Fail("Injecting unsafe shutdown failed: {}"
                                  .format(e.output))


class UnsafeShutdown:
    """
    Utility class specifiying high-level workflow for handling unsafe shutdown.
    "tool" is a class representing specific application used for reading
    and injecting unsafe shutdown and should implement
    following methods:

    get_dev_dimms(dev) - return dimms of device

    inject_usc(dimm) - inject unsafe shutdown into dimm

    read_usc(dimm) - read unsafe shutdown count from dimm

    Dimms returned from get_dev_dimms should constitute a valid input
    to read_usc and inject_usc.
    """
    def __init__(self, tool=None):
        if tool:
            self.tool = tool
        # set default injecting tool
        else:
            if sys.platform == 'win32':
                self.tool = Ipmctl()
            else:
                self.tool = tools.Ndctl()

    def read(self, testdir):
        """
        Read unsafe shutdown count of provided device, which is a sum
        of unsafe shutdown counts of all underlying dimms
        """
        dev = get_dev_from_testdir(testdir)
        dimms = self.get_dev_dimms(dev)
        return sum(self.read_from_dimms(*dimms))

    def inject(self, testdir):
        """
        Inject unsafe shutdown into all dimms used by provided test directory
        """
        dev = get_dev_from_testdir(testdir)
        dimms = self.get_dev_dimms(dev)
        self.inject_to_dimms(*dimms)

    def read_from_dimms(self, *dimms):
        """
        Read unsafe shutdown count values as a list
        """
        usc = []

        for d in dimms:
            usc.append(self.tool.read_usc(d))

        return usc

    def inject_to_dimms(self, *dimms):
        """
        Inject unsafe shutdown into specific dimms
        """
        for d in dimms:
            self.tool.inject_usc(d)

    def get_dev_dimms(self, dev):
        """
        Get device's underlying dimms. For a particular tool their format
        should be valid as an argument input for all methods.
        """
        return self.tool.get_dev_dimms(dev)
