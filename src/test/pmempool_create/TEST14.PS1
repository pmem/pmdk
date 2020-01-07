# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# pmempool_create/TEST14 -- test pmemobj pool creation with default and custom layout
# using increased verbosity level
#

. ../unittest/unittest.ps1

require_test_type medium

require_fs_type any

setup

$LOG = "out${Env:UNITTEST_NUM}.log"
$ERR_LOG = "err${Env:UNITTEST_NUM}.log"

# test default layout
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v $DIR\pool.obj > $LOG

# test custom layout
expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v --layout=moose $DIR\pool2.obj >> $LOG

# test max layout

# MAX_LAYOUT is equal to PMEMOBJ_MAX_LAYOUT (i.e. 1024) macro's value
# reduced by 1 for terminating null byte
$MAX_LAYOUT = 1023

$LAYOUT = "x" * $MAX_LAYOUT

expect_normal_exit $PMEMPOOL$EXESUFFIX create obj -v --layout=$LAYOUT $DIR/pool3.obj >> $LOG

# test too long layout

$INVALID_LAYOUT= "x" * ($MAX_LAYOUT + 1)

expect_abnormal_exit cmd /c $PMEMPOOL$EXESUFFIX create obj -v --layout=$INVALID_LAYOUT $DIR/pool4.obj `>`> $LOG 2`>$ERR_LOG

check

pass
