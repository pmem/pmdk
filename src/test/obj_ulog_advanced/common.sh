#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

#
# src/test/obj_ulog_advanced/common.sh -- common bits and pieces
#

# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. No dynamic libraries are used nor .static_* builds are available.
COMMON_BUILD_TYPE=debug

function common_require() {
        require_fs_type any
        require_build_type $COMMON_BUILD_TYPE
        require_test_type medium
        require_pmemcheck_version_ge 1 0
        require_pmemcheck_version_lt 2 0
        require_pmreorder
}

function common_setup() {
        ERROR_INJECT=$1

        export PMEMOBJ_LOG_LEVEL=10

        BIN="./obj_ulog_advanced$EXESUFFIX"
        TESTFILE=$DIR/testfile
        ERR_LOG_FILE=err$UNITTEST_NUM.log
        # This value was labourly calculated. Please see the source file for
        # details.
        SLOTS_NUM=60
        PMEMCHECK_CMD="$BIN test_publish $TESTFILE $SLOTS_NUM $ERROR_INJECT"
        PMREORDER_CMD="$BIN test_verify $SLOTS_NUM"
}

function common_init() {
        expect_normal_exit $BIN test_init $TESTFILE
}

function common_record() {
        pmreorder_create_store_log $TESTFILE "$PMEMCHECK_CMD"
}

function common_replay_and_check() {
        ERROR_INJECT=$1

        # skip reordering and checking stores outside of the markers
        DEFAULT_ENGINE=NoReorderNoCheck
        # The accumulative reordering is sufficient considering the nature of
        # the scenario at hand where the key risk is that not all stores
        # will be executed. The order of these stores is irrelevant.
        # Please see the source code for the details of the tested scenario.
        # Note: ReorderFull is too time-consuming for this scenario.
        EXTENDED_MACROS="PMREORDER_PUBLISH=ReorderAccumulative"

        if [ $ERROR_INJECT -eq 0 ]; then
                pmreorder_expect_success $DEFAULT_ENGINE "$EXTENDED_MACROS" "$PMREORDER_CMD"
        else
                pmreorder_expect_failure $DEFAULT_ENGINE "$EXTENDED_MACROS" "$PMREORDER_CMD"
        fi
}
