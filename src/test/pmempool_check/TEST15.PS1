# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# pmempool_check/TEST15 -- test for checking poolset with valid parts but with
#	invalid links
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type any

setup

$POOLSET="$DIR\pool.set"
$POOLSET1="$DIR\pool1.set"
$POOLSET2="$DIR\pool2.set"
$PART1="$DIR\pool.part1"
$PART2="$DIR\pool.part2"
$PART3="$DIR\pool.part3"
$PART4="$DIR\pool.part4"
$PARTS=($PART1,$PART2,$PART3,$PART4)

$LOG="out$Env:UNITTEST_NUM.log"
rm $LOG -Force -ea si

foreach ($args in "log","blk","obj") {
	if ($args -eq "blk") {
		echo "Arguments: create $args 512" >> $LOG
	} else {
		echo "Arguments: create $args" >> $LOG
	}

	if ($args -eq "obj") {
		$input_types=(1..2)
		$output_types=(1..3)
	} else {
		$input_types=(1)
		$output_types=(1)
	}

	foreach ($input_type in $input_types) {

		switch ($input_type) {
			1 {
				echo "Input: two parts per poolset" >> $LOG
				create_poolset $POOLSET1 32M:$PART1 32M:$PART2
				create_poolset $POOLSET2 32M:$PART3 32M:$PART4
			}
			2 {
				echo "Input: two replicas per poolset" >> $LOG
				create_poolset $POOLSET1 32M:$PART1 R 32M:$PART2
				create_poolset $POOLSET2 32M:$PART3 R 32M:$PART4
			}
		}

		foreach ($output_type in $output_types) {

			switch ($output_type) {
				1 {
					echo "Output: four parts in poolset" >> $LOG
					create_poolset $POOLSET 32M:$PART1 32M:$PART2 `
						32M:$PART3 32M:$PART4
				}
				2 {
					echo "Output: two replicas with two parts each in poolset" >> $LOG
					create_poolset $POOLSET 32M:$PART1 32M:$PART2 `
						R 32M:$PART3 32M:$PART4
				}
				3 {
					echo "Output: four replicas in poolset" >> $LOG
					create_poolset $POOLSET 32M:$PART1 R 32M:$PART2 `
						R 32M:$PART3 R 32M:$PART4
				}
			}

			rm $PARTS -Force -ea si

			if ($args -eq "blk") {
				expect_normal_exit $PMEMPOOL create $args 512 $POOLSET1
				expect_normal_exit $PMEMPOOL create $args 512 $POOLSET2
			} else {
				expect_normal_exit $PMEMPOOL create $args $POOLSET1
				expect_normal_exit $PMEMPOOL create $args $POOLSET2
			}
			check_files $PART1 $PART2 $PART3 $PART4

			expect_abnormal_exit $PMEMPOOL check -v $POOLSET >> $LOG
			expect_abnormal_exit $PMEMPOOL check -vry $POOLSET >> $LOG
			expect_abnormal_exit $PMEMPOOL check -vary $POOLSET >> $LOG
		}
	}
}

check

pass
