# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation

. ..\unittest\unittest.ps1

require_test_type medium

setup

expect_normal_exit $ENV:EXE_DIR\libpmempool_check_version$Env:EXESUFFIX

check

pass
