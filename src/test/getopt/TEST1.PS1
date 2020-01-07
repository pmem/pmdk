# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/getopt/TEST1 -- unit test for windows getopt implementation
#

#$ERR="stderr$Env:UNITTEST_NUM.log"
. ../unittest/unittest.ps1

require_test_type medium

require_fs_type none

setup
#
#XXX: fix stderror parse on windows (find way to remove PS stderr overlay)
#expect_normal_exit $Env:EXE_DIR\getopt$Env:EXESUFFIX -Z --arg_Z -A 2> $ERR
#
#check

pass
