#
# Copyright 2018, Intel Corporation
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
# src/test/pmempool_feature/common.ps1 -- common part of pmempool_feature tests
#

$PART_SIZE= 10 * 1024 * 1024 # 10MiB
$PART_SIZE_STR = ${PART_SIZE}.toString() + "B" #10MiB
$POOLSET="$DIR\testset"

# create poolset
create_poolset $POOLSET `
	${PART_SIZE_STR}:$DIR\testfile11:x ${PART_SIZE_STR}:$DIR\testfile12:x `
	R ${PART_SIZE_STR}:$DIR\testfile21:x ${PART_SIZE_STR}:$DIR\testfile22:x `
	R ${PART_SIZE_STR}:$DIR\testfile31:x

$LOG = "grep${Env:UNITTEST_NUM}.log"
remove_files $LOG

$exit_func="expect_normal_exit"

# pmempool_feature_query -- query feature
#
# usage: pmempool_feature_query <feature>
function pmempool_feature_query($arg1) {
	$val=$(expect_normal_exit $PMEMPOOL feature -q $arg1 $POOLSET)
	echo "query $arg1 result is $val" >> $LOG
}

# pmempool_feature_enable -- enable feature
#
# usage: pmempool_feature_enable <feature> [no-query]
function pmempool_feature_enable($arg1, $arg2) {
	& $exit_func $PMEMPOOL feature -e $arg1 $POOLSET 2>&1 | Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "x$arg2" -ne "xno-query" ){
		pmempool_feature_query $arg1
	}
}

# pmempool_feature_disable -- disable feature
#
# usage: pmempool_feature_disable <feature> [no-query]
function pmempool_feature_disable($arg1, $arg2) {
	& $exit_func $PMEMPOOL feature -d $arg1 $POOLSET 2>&1 | Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "x$arg2" -ne "xno-query" ){
		pmempool_feature_query $arg1
	}
}
