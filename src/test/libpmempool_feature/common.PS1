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
# src/test/libpmempool_feature/common.ps1 -- common part of libpmempool_feature tests
#

$OUT = "out${Env:UNITTEST_NUM}.log"
$LOG = "grep${Env:UNITTEST_NUM}.log"
remove_files $LOG

$QUERY_PATTERN="query"
$ERROR_PATTERN="<1> \[feature.c:.*\]"

$exit_func="expect_normal_exit"

# libpmempool_feature_query -- query feature
#
# usage: libpmempool_feature_query <enum-pmempool_feature>
function libpmempool_feature_query($arg1) {
	# query feature
	expect_normal_exit $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $DIR\pool.obj q $arg1
	cat $OUT | Select-String "$QUERY_PATTERN" | %{$_.Line} >> $LOG

	# verify query by pmempool info
	$count=(expect_normal_exit $PMEMPOOL info $DIR\pool.obj | Select-String "$arg1").length
	if ( "$count" -eq "0" ){
		echo "pmempool info: $arg1 is NOT set" >> $LOG
	}else{
		echo "pmempool info: $arg1 is set" >> $LOG
	}

	# check if pool is still valid
	expect_normal_exit $PMEMPOOL check $DIR\pool.obj | out-null
}

# libpmempool_feature_enable -- enable feature
#
# usage: libpmempool_feature_enable <enum-pmempool_feature> [no-query]
function libpmempool_feature_enable($arg1, $arg2) {
	& $exit_func $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $DIR\pool.obj e $arg1 2>&1 `
		| Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "$exit_func" -eq "expect_abnormal_exit" ) {
		cat $Env:PMEMPOOL_LOG_FILE | Select-String "$ERROR_PATTERN" | %{$_.Line} >> $LOG
	}
	if ( "$arg2" -ne "no-query" ){
		libpmempool_feature_query $arg1
	}
}

# libpmempool_feature_disable -- disable feature
#
# usage: libpmempool_feature_disable <enum-pmempool_feature> [no-query]
function libpmempool_feature_disable($arg1, $arg2) {
	& $exit_func $Env:EXE_DIR\libpmempool_feature$Env:EXESUFFIX $DIR\pool.obj d $arg1 2>&1 `
		| Select-String "$arg1" | %{$_.Line} >> $LOG
	if ( "$exit_func" -eq "expect_abnormal_exit" ) {
		cat $Env:PMEMPOOL_LOG_FILE | Select-String "$ERROR_PATTERN" | %{$_.Line} >> $LOG
	}
	if ( "$arg2" -ne "no-query" ){
		libpmempool_feature_query $arg1
	}
}
