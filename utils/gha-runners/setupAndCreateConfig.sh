#!/usr/bin/env bash
#
# Copyright 2019, Intel Corporation
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


function create_namespace_and_config() {
    echo "Setup & create config"
    conf_path="${1}"
    test_type_path="${2}"
    test_type_parameter="${3}"
    gha_utils_path="${4}"
    

 	mkdir --parents ${conf_path}
 	${gha_utils_path}/createNamespaceConfig.sh -${test_type_parameter} --conf-pmdk-nondebug-lib-path=${nondebug_lib_path} --conf-path_0=${conf_path}

    echo "${test_type_path}"

    cat ${conf_path}/${test_type_path}

}



while getopts b: flag
do
    case "${flag}" in
        b) badblock=${OPTARG};;
    esac
done

echo "Start setup and create config script";

echo "badblock: $badblock";


FULL_PATH=$(readlink -f .)
PMDK_0_PATH=$(dirname $FULL_PATH)
PMDK_PATH=${PMDK_0_PATH}/pmdk
echo "Fullpath"
echo ${PMDK_PATH}


create_namespace_and_config "$PMDK_PATH/src/test" 'testconfig.sh' 'u' "${PMDK_PATH}/utils/gha-runners"
if [  $badblock == 'yes' ]
then
    echo "BADBLOCK_TEST_TYPE=real_pmem" >> $PMDK_PATH/src/test/testconfig.sh
 	echo "TEST_TIMEOUT=20m" >> $PMDK_PATH/src/test/testconfig.sh
fi


