# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2019, Intel Corporation

#
# src/test/getopt/TEST0 -- unit test for windows getopt implementation
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type none

setup

$ERR="stderr${Env:UNITTEST_NUM}.log"

expect_normal_exit $Env:EXE_DIR\getopt$Env:EXESUFFIX nonOption -f -3arg `
-Barg -b --arg_4=a --arg_G -5 -a -A arg --arg_g -d -1 -e nonOption2 --arg_8 `
nonOption3 --arg_6 -E-a --arg_C=arg -a -7arg --arg_c --arg_D arg -h --arg_H `
arg -2 nonOption4 2> $ERR

check

pass
