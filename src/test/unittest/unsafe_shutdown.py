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
    GET_DISK_NO_CMD = r'powershell (Get-Partition -DriveLetter'
    r'(Get-Item {}:\).PSDrive.Name).DiskNumber'
    GET_DIMMS_FROM_DISK_NO = 'powershell Get-PmemDisk | Where DiskNumber -Eq '
    '{} | Format-Table -Property PhysicalDeviceIds -HideTableHeaders -Autosize'

    def dev_from_testdir(testdir):
        """
        Get device ID (disk number) form test directory
        using PowerShell cmdlets
        """
        volume, _ = os.path.splitdrive(testdir)
        print(volume)
        cmd = GET_DISK_NO_CMD.format(volume.rstrip(':'))
        cmd = shlex.split(cmd)
        proc = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT,
                      universal_newlines=True)
        if proc.returncode:
            futils.Fail('The {} disk number could not be found'.format(volume))
        return proc.stdout.strip()

    def get_dimms_from_disk_no(disk_no):
        """
        Get device ID (disk number) form test directory
        using PowerShell cmdlets
        """
        cmd = GET_DIMMS_FROM_DISK_NO.format(disk_no)
        cmd = shlex.split(cmd)
        proc = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT,
                      universal_newlines=True)
        if proc.returncode:
            futils.Fail('The {} disk number could not be found'
                        .format(proc.stdout))
        return proc.stdout.strip().strip(r'{}').split(',')

else:

    def dev_from_testdir(testdir):
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
    def __init__(self):
        pass

    def get_dev_dimms(self, dev):
        return get_dimms_from_disk_no(dev)

    def read_usc(self, *dimms):
        usc = 0
        cmd_format = 'ipmctl show -sensor -dimm 0x{}'
        row_title = 'LatchedDirtyShutdownCount'
        for d in dimms:
            out = ''
            cmd = cmd_format.format(d)
            try:
                out = sp.check_output(cmd.split(), universal_newlines=True,
                                      stderr=sp.STDOUT)
                usc_row = [o for o in out.splitlines() if row_title in o][0]

                # assuming 'usc_row' looks like:
                # "0x0101 | LatchedDirtyShutdownCount   | 4           | Normal"
                # 'usc' is the value in the third column:
                usc += int(usc_row.split('|')[2].strip())
            except sp.CalledProcessError as e:
                raise futils.Fail('Reading unsafe shutdown count '
                                  'with ipmctl failed:\n{}'.format(e.output))
            except (IndexError, ValueError):
                raise futils.Fail('Could not read dirty shutdown'
                                  'value from dimm {}. \n{}'
                                  .format(d, out))

        return usc

    def inject_usc(self, *dimms):
        cmd_format = 'ipmctl set -dimm 0x{} DirtyShutdown=1'
        for d in dimms:
            try:
                cmd = cmd_format.format(d).split()
                sp.check_output(cmd, universal_newlines=True,
                                stderr=sp.STDOUT)
            except sp.CalledProcessError as e:
                raise futils.Fail("Injecting unsafe shutdown failed: {}"
                                  .format(e.output))


class UnsafeShutdown:
    def __init__(self, tool=None):
        # set default injecting tool
        if not tool:
            if sys.platform == 'win32':
                self.tool = Ipmctl()
            else:
                self.tool = tools.Ndctl()

    def inject(self, *dimms):
        for d in dimms:
            self.tool.inject_usc(d)

    def read(self, *dimms):
        usc = 0

        for d in dimms:
            usc += self.tool.read_usc(d)

        return usc

    def get_dev_dimms(self, dev):
        return self.tool.get_dev_dimms(dev)
