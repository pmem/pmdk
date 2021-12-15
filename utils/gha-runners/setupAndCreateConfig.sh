#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2014-2020, Intel Corporation

#
# setupAndCreateConfig.sh - Script for gathering input arguments for namespace creation
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
echo "Fullpath:" ${PMDK_PATH}


create_namespace_and_config "$PMDK_PATH/src/test" 'testconfig.sh' 'u' "${PMDK_PATH}/utils/gha-runners"
if [  $badblock == 'yes' ]
then
    echo "BADBLOCK_TEST_TYPE=real_pmem" >> $PMDK_PATH/src/test/testconfig.sh
 	echo "TEST_TIMEOUT=20m" >> $PMDK_PATH/src/test/testconfig.sh
fi


