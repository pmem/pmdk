# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# libpmempool_api/TEST9 -- test for checking API
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

require_short_path

setup

$POOL="$DIR\file.pool"
$LOG="out${Env:UNITTEST_NUM}.log"
$LOG_TEMP="out${Env:UNITTEST_NUM}_part.log"

rm $LOG -Force -ea si
rm $LOG_TEMP -Force -ea si
touch $LOG_TEMP

$pool_args=@{"blk"="blk 512"; "log"="log"}

foreach ($h in $pool_args.GetEnumerator()) {
	echo "Pool type: $($h.Name) Params: $($h.Value)" | out-file -append -encoding ascii -literalpath $LOG_TEMP
	expect_normal_exit "$PMEMPOOL create $($h.Value) $POOL" >> $LOG_TEMP
	&$PMEMSPOIL $POOL "pool_hdr.major=7" >> $LOG_TEMP
	check_file $POOL

	cp $POOL ${POOL}_copy >> $LOG_TEMP
	expect_normal_exit $Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX `
		-d 1 -r 1 -y 1 -t $($h.Name) $POOL
	cat -Encoding Ascii $LOG | out-file -append -encoding ascii -literalpath $LOG_TEMP
	cmp $POOL ${POOL}_copy >> $LOG_TEMP

	rm $POOL -Force -ea si
	rm ${POOL}_copy -Force -ea si
}
rm $LOG -Force
mv $LOG_TEMP $LOG

check
pass
