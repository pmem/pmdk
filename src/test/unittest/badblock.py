# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2021-2023, Intel Corporation
#
"""Bad block utilities"""

import abc
import sys

import futils
import tools


class BBTool(abc.ABC):
    """
    An abstract class implementing an interface necessary to serve as an
    bad block handler tool for public BadBlock class.
    """

    @abc.abstractmethod
    def inject_bad_block(self, file, offset):
        """
        Inject an error into the namespace of the specified file at the
        provided location.
        """
        pass

    @abc.abstractmethod
    def get_bad_blocks_count(self, file):
        """
        Returns the number of bad blocks in the namespace of
        the specified file.
        """
        pass

    @abc.abstractmethod
    def clear_all_bad_blocks(self, file):
        """
        Clears all bad blocks of a file.
        """
        pass


class BadBlock:
    """
    Utility class specifying high-level workflow for handling bad block.
    """
    def __init__(self, util_tools, tool: BBTool = None):
        if tool:
            if not isinstance(tool, BBTool):
                raise futils.Fail('{} should be a subclass of BBTool'
                                  .format(tool))
            self.tool = tool
        else:
            if sys.platform == 'win32':
                raise futils.Fail('no bad block implementation on win32')
            else:
                self.tool = NdctlBB(util_tools)

    def inject(self, file, offset):
        self.tool.inject_bad_block(file, offset)

    def get_count(self, file):
        return self.tool.get_bad_blocks_count(file)

    def clear_all(self, file):
        self.tool.clear_all_bad_blocks(file)


class NdctlBB(tools.Ndctl, BBTool):
    """ndctl based bad-block tool"""

    def __init__(self, util_tools):
        super().__init__()
        self.util_tools = util_tools

    def inject_bad_block(self, file, offset):
        device = self._get_path_device(file)
        namespace = self.get_dev_namespace(device)

        count = 1

        if self.is_devdax(device):
            # for testing purposes devdax is treated as a file itself,
            # therefore the offset is a physical offset on the device
            block = offset
        else:
            # fsdax relies on the fs for block allocation, so a virtual
            # offset must be translated into a physical one with a tool
            proc = self.util_tools.extents("-l {}".format(offset), file)
            if proc.returncode != 0:
                raise futils.Fail('unable to translate bad blocks')
            block = int(proc.stdout.strip())

        futils.run_command("sudo ndctl inject-error --block={} --count={} {}"
                           .format(block, count, namespace),
                           "injecting bad block failed")

    def get_bad_blocks_count(self, file):
        device = self._get_path_device(file)
        return self.get_dev_bb_count(device)

    def clear_all_bad_blocks(self, file):
        device = self._get_path_device(file)
        namespace = self.get_dev_namespace(device)

        if self.is_devdax(device):
            futils.run_command("sudo ndctl clear-errors {}".format(namespace),
                               "failed to clear bad blocks")
            # The "sudo ndctl clear-errors" command resets permissions
            # of the DAX block device and its resource files
            # because it removes the old files and creates new ones
            # with default permissions which interrupts further test execution.
            # The following commands set permissions which should allow
            # further test execution to work around this issue.
            futils.run_command("sudo chmod a+rw {}".format(device),
                               "failed to change permissions of a DAX device")
            resources = "/sys/bus/nd/devices/ndbus*/region*/dax*/resource"
            futils.run_command("sudo chmod a+r {}".format(resources),
                               "failed to change permissions "
                               "of DAX devices' resources")
        else:
            out = futils.run_command("mount | grep {}".format(device),
                                     "querying mount points failed")
            mount = out.strip().decode('UTF8').split()[2]

            futils.run_command("sudo umount {}".format(mount),
                               "unmounting device failed")

            futils.run_command("sudo ndctl clear-errors {}".format(namespace),
                               "clearing bad blocks failed")

            futils.run_command("sudo mount -o dax {} {}".format(device, mount),
                               "mounting device failed")
