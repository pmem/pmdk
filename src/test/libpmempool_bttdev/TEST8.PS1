# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2019, Intel Corporation
#
#
# libpmempool_bttdev/TEST8 -- test for checking btt
#

. ..\unittest\unittest.ps1

require_test_type medium
require_fs_type any

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
$LOG_TEMP="out${Env:UNITTEST_NUM}_part.log"
rm $LOG -Force -ea si
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP
$EXE="$Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX"

expect_normal_exit $BTTCREATE $POOL >> $LOG

$btt_info_dic_err = @{	"sig"="ERROR"
						"uuid"="01-02"
						"flags"="7"
						"major"="7"
						"minor"="7"
						"nfree"="7"
						"infosize"="7"
						"nextoff"="7"
						"dataoff"="7"
						"infooff"="7"
						"unused"="7"
						"parent_uuid"="03-04"
						"mapoff"="7"
						"flogoff"="7"	}

foreach ($field in ("flags", "unused", "major", "sig", "nextoff", "infosize", "infooff", "dataoff", "nfree", "mapoff", "uuid", "parent_uuid", "flogoff", "minor")) {

	$x = $btt_info_dic_err[$field]

	$spcmd="bttdevice.arena(0).btt_info.$field=${x}"

	rm $POOL -Force -ea si

	expect_normal_exit $BTTCREATE $POOL

	expect_normal_exit $PMEMSPOIL -v $POOL $spcmd >> $LOG_TEMP

	expect_normal_exit $EXE -r 1 -t btt $POOL

	cat -Encoding Ascii $LOG | out-file -append -literalpath $LOG_TEMP
	}

check_file $POOL

mv -Force $LOG_TEMP $LOG

check

pass
