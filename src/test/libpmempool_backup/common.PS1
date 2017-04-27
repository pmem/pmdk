#
# Copyright 2016, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#
# libpmempool_backup/common.PS1 -- functions for libpmempool_backup unittest
#
$POOLSET="$DIR/pool.set"
$BACKUP="_backup"
$REPLICA="_replica"
$POOL_PART="$DIR/pool.part"

$OUT="out$Env:UNITTEST_NUM.log"
$OUT_TEMP="out$Env:UNITTEST_NUM_temp.log"
$DIFF="diff$Env:UNITTEST_NUM.log"
Remove-Item $DIFF, $OUT_TEMP -Recurse -Force -ea si
touch $DIFF $OUT_TEMP

# params for blk, log and obj pools
Set-Variable -Scope Script -Name POOL_TYPES -Value @( "blk", "log", "obj" )
Set-Variable -Scope Script -Name POOL_CREATE_PARAMS -Value @( @("--write-layout", "512"), "", @("--layout", "test_layout"))
Set-Variable -Scope Script -Name POOL_CHECK_PARAMS -Value @( "-smgB", "-s", @("-soOaAbZH", "-l", "-C"))

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
	Get-Content -Encoding Ascii $OUT | Out-File -append -encoding ascii $OUT_TEMP

	# compare
	if ($args.Count -gt 2 ) {
        $check=$args[2]
		compare_replicas $check $poolset$BACKUP $poolset >> $DIFF
	}
}

Set-Variable -Scope Script -Name ALL_POOL_PARTS -Value `
	"${POOL_PART}1, ${POOL_PART}2, ${POOL_PART}3, ${POOL_PART}4, `
	${POOL_PART}${REPLICA}"
Set-Variable -Scope Script -Name ALL_POOL_BACKUP_PARTS -Value `
	"${POOL_PART}1$BACKUP, ${POOL_PART}2$BACKUP, `
	${POOL_PART}3$BACKUP ${POOL_PART}4$BACKUP, `
	${POOL_PART}${BACKUP}${REPLICA}"

#
# backup_cleanup -- perform cleanup between test cases
#
function backup_cleanup() {
    Remove-Item $POOLSET$BACKUP, ${POOL_PART}1, ${POOL_PART}2, ${POOL_PART}3, `
        ${POOL_PART}4, ${POOL_PART}${REPLICA} -ea si

    Remove-Item ${POOL_PART}1$BACKUP, ${POOL_PART}2$BACKUP, ${POOL_PART}3$BACKUP, `
        ${POOL_PART}4$BACKUP, ${POOL_PART}$BACKUP${REPLICA} -ea si
}
