# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020-2023, Intel Corporation
#
"""Various requirements"""

import os
from shutil import which
import subprocess as sp

import configurator as conf
import context as ctx
import futils


NDCTL_MIN_VERSION = '63'


class Requirements:
    """Various requirements"""
    def __init__(self):
        self.cfg = conf.Configurator().config

    def _check_pkgconfig(self, pkg, min_ver):
        """
        Run pkg-config to check if a package is installed
        """
        proc = sp.run(['pkg-config', pkg, '--atleast-version', min_ver],
                      stdout=sp.PIPE, stderr=sp.STDOUT)
        return proc.returncode == 0

    def _is_ndctl_enabled(self):
        path = futils.get_tool_path(ctx, "pmempool")
        s = sp.check_output(["strings", path])
        if "compiled with libndctl" in str(s):
            return True

        return False

    def _check_is_admin(self):
        cmd = "sudo -n id -u"
        fail_msg = "checking user id failed"

        cmd_output = futils.run_command(cmd, fail_msg)
        cmd_output = cmd_output.strip().decode('UTF8')
        if cmd_output != '0':
            return False

        return True

    def check_ndctl(self):
        is_ndctl = self._check_pkgconfig('libndctl', NDCTL_MIN_VERSION)
        if not is_ndctl:
            raise futils.Skip('libndctl (>=v{}) is not installed'
                              .format(NDCTL_MIN_VERSION))

    def check_ndctl_enable(self):
        if self._is_ndctl_enabled() is False:
            raise futils.Skip('ndctl is disabled - binary not '
                              'compiled with libndctl')

    def check_namespace(self):
        cmd = ['ndctl', 'list']
        cmd_as_str = ' '.join(cmd)
        try:
            proc = sp.run(cmd, stdout=sp.PIPE, stderr=sp.STDOUT,
                          universal_newlines=True)
        except (OSError):
            raise futils.Fail('ndctl is not installed')
        if proc.returncode != 0:
            raise futils.Fail('"{}" failed:{}{}'.format(cmd_as_str, os.linesep,
                                                        proc.stdout))
        if not proc.stdout or proc.stdout.isspace():
            raise futils.Skip('no ndctl namespace set')

    def _check_ndctl_req_is_met(self, tc):
        """
        Check if all conditions for the ndctl requirement are met
        """
        require_ndctl, kwargs = ctx.get_requirement(tc, 'require_ndctl', ())
        if not require_ndctl:
            return True

        self.check_ndctl_enable()
        self.check_ndctl()

        if kwargs.get('require_namespace', False):
            self.check_namespace()

        return True

    def _check_admin_req_is_met(self, tc):
        """
        Check if all conditions for the admin requirement are met
        """
        require_admin, _ = ctx.get_requirement(tc, 'require_admin', ())
        if not require_admin:
            # admin is not required
            return True

        if not self.cfg.enable_admin_tests:
            raise futils.Skip('admin tests are not enabled in config '
                              '(enable_admin_tests)')

        if not self._check_is_admin():
            raise futils.Fail('Error: admin tests are enabled in config, '
                              'but the user does not have administrative '
                              'privileges')
        # user is admin
        return True

    def check_if_all_requirements_are_met(self, tc):
        if not self._check_ndctl_req_is_met(tc):
            return False
        if not self._check_admin_req_is_met(tc):
            return False

        """More requirements can be checked here"""
        return True


def require_ndctl(**kwargs):
    """
    Add requirement to run test only if ndctl is installed.
    Optionally a namespace requirement can be enabled.

    Used as a test class decorator.

    Args:
        kwargs: optional keyword arguments
    """
    valid_kwarg_keys = ['require_namespace']

    # check if all provided keys are valid
    for key in kwargs.keys():
        if key not in valid_kwarg_keys:
            raise KeyError('provided key {} is invalid'.format(key))

    # namespace requirement
    namespace_kw = 'require_namespace'
    namespace_val = kwargs.get(namespace_kw, False)
    if not isinstance(namespace_val, bool):
        raise ValueError('provided value {} for optional key {} is invalid'
                         .format(namespace_val, namespace_kw))

    def wrapped(tc):
        ctx.add_requirement(tc, 'require_ndctl', True, **kwargs)
        return tc
    return wrapped


def require_admin(tc):
    """
    Disable test if "enable_admin_tests" configuration is not set
    """
    ctx.add_requirement(tc, 'require_admin', True)
    return tc


def require_command(command):
    """
    Disable test if given command is not provided
    """
    def wrapped(tc):
        if which(command) is None:
            tc.enabled = False
        return tc
    return wrapped
