# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#
"""Various requirements"""


import os
import sys
import ctypes


import configurator as conf
import context as ctx
import futils


def util_is_admin():
    if sys.platform == 'win32':
        return ctypes.windll.shell32.IsUserAnAdmin() != 0
    else:
        return os.getuid() == 0


def require_admin(**kwargs):
    """
    Disable test if "enable_admin_tests" configuration is not set
    """
    def wrapped(tc):
        ctx.add_requirement(tc, 'require_admin', True)
        return tc

    return wrapped


def require_admin_is_met(tc):
    """
    Check if all conditions for the admin requirement are met
    """
    config = conf.Configurator().config
    msg = futils.Message(config.unittest_log_level)
    require_admin, _ = ctx.get_requirement(tc, 'require_admin', ())

    if not require_admin:
        # admin is not required
        return True

    if not config.enable_admin_tests:
        msg.print_verbose('{}: SKIP: admin tests are not enabled in config'
                          ' (enable_admin_tests)'.format(tc))
        return False

    if util_is_admin():
        # user is admin
        return True

    msg.print('{}: FAIL: admin tests are enabled in config,'
              ' but the user does not have administrative privileges'
              .format(tc))
    sys.exit(1)
