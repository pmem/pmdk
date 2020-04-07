# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, Intel Corporation
#

"""Fs context classes and utilities"""

import granularity as gr


def require_fs(granularity=None):
    """
    Set filesystem requirements for a test case.

    Should be used as a class decorator.

    Args:
        granularity (optional): required granularity.
            Should be either a single or a tuple of
            _Granularity type values.

    """
    if isinstance(granularity, list):
        raise ValueError('Use tuple instead of list as a kwarg value')
    elif isinstance(granularity, gr._Granularity):
        granularity = (granularity,)

    def wrapped(tc):
        if granularity is not None:
            gr.require_granularity(tc, *granularity)
        return tc
    return wrapped
