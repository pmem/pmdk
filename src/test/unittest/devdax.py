# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2023, Intel Corporation
#

"""Device dax context classes and utilities"""

import copy
import itertools
import os
import sys

import context as ctx
import futils
import requirements as req
import tools


class DevDax():
    """
    Class representing dax device and its parameters.

    Attributes:
        name (str): name of the DevDax object by which it is
            accessible from DaxDevices class
        path (str): path to device
        _req_alignment (int): required dax device alignment
        _req_min_size (int): required dax device minimal size
        _req_max_size (int): required dax device maximal size
        assigned (bool): True, if object was successfully assigned
            to real dax device that fulfills its requirements,
            False otherwise

    """
    def __init__(self, name, alignment=None, min_size=None, max_size=None):
        self.name = name
        self.path = None
        self._req_alignment = alignment
        self._req_min_size = min_size
        self._req_max_size = max_size
        self.assigned = False

    def __str__(self):
        return self.name

    def try_assign(self, path):
        """
        Try assigning to real dax device, identified by its path,
        provided it meets defined requirements. In case of success, set DevDax
        object attributes (like size and/or alignment) to the real dax device
        values and return True. Return False otherwise.
        """
        ndctl = tools.Ndctl()

        p_size = ndctl.get_dev_size(path)
        p_align = ndctl.get_dev_alignment(path)

        if self._req_min_size and p_size < self._req_min_size:
            return False
        if self._req_max_size and p_size > self._req_max_size:
            return False
        if self._req_alignment and p_align != self._req_alignment:
            return False

        self.path = path
        self.size = p_size
        self.alignment = p_align
        self.assigned = True
        return True


class DevDaxes():
    """
    Dax device context element class representing a set of dax devices required
    for the test.

    Attributes:
        dax_devices (tuple): set of dax devices (DevDax objects)
            which are required by the test.
            Each DevDax is accessible from DaxDevices
            as an attribute through its name

    """
    check_fs_exec = False

    def __init__(self, *dax_devices):
        self.dax_devices = tuple(dax_devices)
        for dd in dax_devices:
            setattr(self, dd.name, dd)

    def setup(self, **kwargs):
        # DevDax tests always require libndctl
        req.Requirements().check_ndctl_enable()
        req.Requirements().check_libndctl()
        req.Requirements().check_ndctl()

        tools = kwargs['tools']
        for dd in self.dax_devices:
            proc = tools.pmemdetect('-d', dd.path)
            if proc.returncode != 0:
                raise futils.Fail('checking {} with pmemdetect failed:{}{}'
                                  .format(dd.path, os.linesep, proc.stdout))

            if self.check_fs_exec:
                exec_allowed = tools.mapexec(dd.path)
                if exec_allowed.returncode != 1:
                    raise futils.Skip('dax device {} has no exec rights for'
                                      ' mmap'.format(dd.path))

    def __str__(self):
        return 'devdax'

    @classmethod
    def filter(cls, config, msg, tc):
        """
        Initialize DevDaxes class for the test to be run
        based on configuration and test requirements.

        Args:
            config: configuration as returned by Configurator class
            msg (Message): level based logger class instance
            tc (BaseTest): test case, from which the dax device run
                requirements are obtained

        Returns:
            Initialized DevDaxes class as single element list for type
            compliance

        """
        dax_devices, _ = ctx.get_requirement(tc, 'devdax', ())
        cls.check_fs_exec, _ = ctx.get_requirement(tc, 'require_fs_exec',
                                                   None)
        req_real_pmem, _ = ctx.get_requirement(tc, 'require_real_pmem', False)

        if not dax_devices:
            return ctx.NO_CONTEXT
        elif sys.platform == 'win32' and tc.enabled:
            raise futils.Fail('dax device functionality required by "{}" is '
                              'not available on Windows. Please disable the '
                              'test for this platform'.format(tc))

        if not config.device_dax_path:
            raise futils.Skip('No dax devices defined in testconfig')

        if len(dax_devices) > len(config.device_dax_path):
            raise futils.Skip('Not enough dax devices defined in testconfig '
                              '({} needed)'.format(len(dax_devices)))

        ndctl = tools.Ndctl()
        for c in config.device_dax_path:
            if not ndctl.is_devdax(c):
                raise futils.Fail('{} is not a dax device'.format(c))

        assigned = _try_assign_by_requirements(config.device_dax_path,
                                               dax_devices)

        # filter out the emulated devdaxes from the assigned ones
        if req_real_pmem:
            assigned_filtered = []

            for dd in assigned:
                if not ndctl.is_emulated(dd.path):
                    assigned_filtered.append(dd)

            assigned = tuple(assigned_filtered)

        if not assigned:
            raise futils.Skip('Dax devices in test configuration do not '
                              'meet test requirements')

        return [DevDaxes(*assigned)]


def _try_assign_by_requirements(configured, required):
    """
    Try assigning dax devices defined as paths in test configuration to
    dax device objects taking their requirements into account.
    Return a sequence of all requirement objects if they were
    successfully assigned to existing dax, otherwise return None.
    Since the order in which requirement objects are tried to be
    assigned may affect the final outcome, all permutations are checked.
    """
    permutations = itertools.permutations(required)
    for p in permutations:
        conf_copy = configured[:]
        req_copy = copy.deepcopy(p)
        for dd in req_copy:
            for i, c in enumerate(conf_copy):
                if dd.try_assign(c):
                    conf_copy.pop(i)
                    break

            if not dd.assigned:
                # at least one device dax requirement cannot be assigned
                # to any of devices defined in the configuration,
                # try another permutation
                break

        if all(dd.assigned for dd in req_copy):
            return req_copy
    return None


def require_devdax(*dax_devices, **kwargs):
    """
    Add requirement to run test on specified dax devices.

    Used as a test class decorator.

    Args:
        dax_devices: variable length list of initialized DevDax objects
        kwargs: optional keyword arguments
    """
    def wrapped(tc):
        ctx.add_requirement(tc, 'devdax', tuple(dax_devices), **kwargs)
        return tc
    return wrapped


def require_fs_exec(tc):
    """
    Disable test if exec access for device is not permitted
    """
    ctx.add_requirement(tc, 'require_fs_exec', True)
    return tc
