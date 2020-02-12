# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/signal_handle/TEST0w.PS1 -- unit test for signal_handle
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none
require_build_type debug

setup

expect_normal_exit $Env:EXE_DIR\signal_handle$Env:EXESUFFIX s a a i v

check

pass
