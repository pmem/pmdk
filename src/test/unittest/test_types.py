# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#
"""Test type context classes"""

import context as ctx


class _TestType(metaclass=ctx.CtxType):
    """Base class for a test duration."""


class Short(_TestType):
    pass


class Medium(_TestType):
    pass


class Long(_TestType):
    pass


class Check(_TestType):
    includes = [Short, Medium]


class All(_TestType):
    includes = [Short, Medium, Long]
