# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019-2020, Intel Corporation
#

"""Test framework public interface"""

import sys
from os import path
sys.path.insert(1, path.abspath(path.join(path.dirname(__file__), 'unittest')))

# flake8 issues silenced:
# E402 - import statements not at the top of the file because of adding
# directory to path
# F401, F403 - testframework.py does not use imported names, only passes them
# down and in most cases needs to pass down all of them - hence import with '*'

from basetest import BaseTest, Test, get_testcases  # noqa: E402, F401
from context import *  # noqa: E402, F401, F403
from configurator import *  # noqa: E402, F401, F403
from valgrind import *  # noqa: E402, F401, F403
from utils import *  # noqa: E402, F401, F403
from poolset import *  # noqa: E402, F401, F403
from builds import *  # noqa: E402, F401, F403
from devdax import *  # noqa: E402, F401, F403
from test_types import *  # noqa: E402, F401, F403
from requirements import *  # noqa: E402, F401, F403
import granularity  # noqa: E402, F401
