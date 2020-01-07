# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_sync/TEST5 -- test for checking pmempool sync;
#                        pmem/issues#275
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

# XXX: Not sure how to deal with the file permissions (chmod) on Windows.

#$LOG = "out${Env:UNITTEST_NUM}.log"
#$LOG_TEMP = "out${Env:UNITTEST_NUM}_part.log"
#rm $LOG -Force -ea si
#touch $LOG
#rm $LOG_TEMP -Force -ea si
#touch $LOG_TEMP

#$LAYOUT = "OBJ_LAYOUT${Env:SUFFIX}"
#$POOLSET = "$DIR\pool0.set"

## Create poolset file
#create_poolset $POOLSET `
#    20M:$DIR\testfile00:x `
#    30M:$DIR\testfile01:x `
#    R `
#    30M:$DIR\testfile10:x `
#    20M:$DIR\testfile11:x

## Create a poolset
#expect_normal_exit $PMEMPOOL create --layout=$LAYOUT obj $POOLSET
#cat $LOG >> $LOG_TEMP

## Delete the second part in the primary replica
#rm $DIR\testfile01 -Force -ea si

## Synchronize replicas
#expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

## Check if restored part file have the same permissions as other parts
#(Get-Item $DIR\testfile00).mode >> $LOG_TEMP
#(Get-Item $DIR\testfile01).mode >> $LOG_TEMP

## Delete the first part in the second replica
#rm $DIR\testfile10 -Force -ea si

# XXX: Need chmod() equivalent on Windows

## Change permissions of all the files
#chmod 600 $DIR\testfile*

## Synchronize replicas
#expect_normal_exit $PMEMPOOL sync $POOLSET >> $LOG_TEMP

## Check if restored part file have the same permissions as other parts
#(Get-Item $DIR\testfile10).mode >> $LOG_TEMP
#(Get-Item $DIR\testfile11).mode >> $LOG_TEMP

#mv -Force $LOG_TEMP $LOG
#check

pass
