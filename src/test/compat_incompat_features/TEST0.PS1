# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017-2019, Intel Corporation
#
#
# compat_incompat_features/TEST0.PS1 -- test for compat/incompat feature flags
#
# check if pool opens when compat flags are equal and nonzero
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

. .\common.PS1

# Create poolset file
create_poolset $POOLSET `
    20M:$DIR\part0:x `
    20M:$DIR\part1:x

Foreach ($type in $POOL_TYPES)
{
    expect_normal_exit $PMEMPOOL rm -f $POOLSET | out-file -append -encoding ascii -literalpath $LOG_TEMP
    expect_normal_exit $PMEMPOOL create $create_args[$type] | out-file -append -encoding ascii -literalpath $LOG_TEMP

    Foreach ($flag in $UNKNOWN_COMPAT)
    {
        # Set compat/incompat flags in headers
        set_compat $DIR\part0 $flag | out-file -append -encoding ascii -literalpath $LOG_TEMP
        set_compat $DIR\part1 $flag | out-file -append -encoding ascii -literalpath $LOG_TEMP

        # Check if pool fails to open
        expect_normal_exit $Env:EXE_DIR\pool_open$Env:EXESUFFIX $type $POOLSET
        cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP
        cat -Encoding Ascii $ERR | out-file -append -encoding ascii -literalpath $ERR_TEMP
    }
}

mv -Force $LOG_TEMP $LOG
mv -Force $ERR_TEMP $ERR

check
pass
