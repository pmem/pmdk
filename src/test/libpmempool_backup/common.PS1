# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016, Intel Corporation
#
#
# libpmempool_backup/common.PS1 -- functions for libpmempool_backup unittest
#
$POOLSET="$DIR\pool.set"
$BACKUP="_backup"
$REPLICA="_replica"
$POOL_PART="$DIR\pool.part"

$OUT="out$Env:UNITTEST_NUM.log"
$OUT_TEMP="out$Env:UNITTEST_NUM_temp.log"
$DIFF="diff$Env:UNITTEST_NUM.log"
rm $DIFF, $OUT_TEMP -Force -ea si
touch $DIFF $OUT_TEMP

# params for blk, log and obj pools
$POOL_TYPES=@( "blk", "log", "obj" )
$POOL_CREATE_PARAMS=@( @("--write-layout", "512"), "", @("--layout", "test_layout"))
$POOL_CHECK_PARAMS=@( "-smgB", "-s", @("-soOaAbZH", "-l", "-C"))
$POOL_OBJ=2

# create_poolset_variation -- create one from the tested poolset variation
#    usage: create_poolset_variation <variation-id> [<suffix>]
#
function create_poolset_variation() {
	$sfx=""
	$variation=$args[0]

	if($args.Count -gt 1 ) {
		$sfx=$args[1]
	}

	switch($variation)
	{
	    1 {
		    # valid poolset file
		    create_poolset $POOLSET$sfx `
			    20M:${POOL_PART}1$sfx`:x `
			    20M:${POOL_PART}2$sfx`:x `
			    20M:${POOL_PART}3$sfx`:x `
			    20M:${POOL_PART}4$sfx`:x
	    }
	    2 {
		    # valid poolset file with replica
		    create_poolset $POOLSET$sfx `
			    20M:${POOL_PART}1$sfx`:x `
			    20M:${POOL_PART}2$sfx`:x `
			    20M:${POOL_PART}3$sfx`:x `
			    20M:${POOL_PART}4$sfx`:x `
			    r 80M:${POOL_PART}${REPLICA}$sfx:x
	    }
	    3 {
		    # other number of parts
		    create_poolset $POOLSET$sfx `
			    20M:${POOL_PART}1$sfx`:x `
			    20M:${POOL_PART}2$sfx`:x `
			    40M:${POOL_PART}3$sfx`:x
	    }
	    4 {
		    # no poolset
                return
	    }
	    5 {
		    # empty
		    create_poolset $POOLSET$sfx
	    }
	    6 {
		    # other size of part
		    create_poolset $POOLSET$sfx `
			    20M:${POOL_PART}1$sfx`:x `
			    20M:${POOL_PART}2$sfx`:x `
			    20M:${POOL_PART}3$sfx`:x `
			    21M:${POOL_PART}4$sfx`:x
	    }
	}

	check_file $POOLSET$sfx
}

#
# backup_and_compare -- perform backup and compare backup result with original
# if compare parameters are provided
#    usage: backup_and_compare <poolset> <type> [<compare-params>]
#
function backup_and_compare () {
	$poolset=$args[0]
	$type=$args[1]

	# backup
	expect_normal_exit $Env:EXE_DIR\libpmempool_test$Env:EXESUFFIX `
		-b $poolset$BACKUP -t $type -r 1 $poolset
	cat -Encoding Ascii $OUT | out-file -append -encoding ascii -literalpath $OUT_TEMP

	# compare
	if ($args.Count -gt 2 ) {
        $check=$args[2]
		compare_replicas $check $poolset$BACKUP $poolset >> $DIFF
	}
}

$ALL_POOL_PARTS="${POOL_PART}1, ${POOL_PART}2, ${POOL_PART}3, ${POOL_PART}4, `
	${POOL_PART}${REPLICA}"
$ALL_POOL_BACKUP_PARTS="${POOL_PART}1$BACKUP, ${POOL_PART}2$BACKUP, `
	${POOL_PART}3$BACKUP ${POOL_PART}4$BACKUP, `
	${POOL_PART}${BACKUP}${REPLICA}"

#
# backup_cleanup -- perform cleanup between test cases
#
function backup_cleanup() {
	rm $POOLSET$BACKUP, ${POOL_PART}1, ${POOL_PART}2, ${POOL_PART}3, `
        ${POOL_PART}4, ${POOL_PART}${REPLICA} -ea si

    rm ${POOL_PART}1$BACKUP, ${POOL_PART}2$BACKUP, ${POOL_PART}3$BACKUP, `
        ${POOL_PART}4$BACKUP, ${POOL_PART}$BACKUP${REPLICA} -ea si
}
