#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

#
# src/test/daos_dtx/common.sh -- common bits and pieces
#

TESTS_BIN=dtx_tests
PMEMOBJ_TESTS_BIN=$(ldd $(which $TESTS_BIN) | grep pmemobj | cut -d ' ' -f 3)
VERIFY_BIN=$(which ddb) # an absolute path is required

# The 'debug' build is chosen arbitrarily to ensure these tests are run only
# once. All used binares are provided externally.
COMMON_BUILD_TYPE=debug

function common_require() {
        require_fs_type any
        require_build_type $COMMON_BUILD_TYPE
        require_test_type medium
        require_command $TESTS_BIN
        require_command $VERIFY_BIN
        if [ ! -e "$PMEMOBJ_TESTS_BIN" ]; then
                msg=$(interactive_red STDOUT "FAIL:")
                echo -e "$UNITTEST_NAME: $msg Can not find LIBPMEMOBJ for $TESTS_BIN"
                exit 1
        fi
        require_pmemcheck_version_ge 1 0 $PMEMOBJ_TESTS_BIN
        require_pmemcheck_version_lt 2 0 $PMEMOBJ_TESTS_BIN
        require_pmreorder $PMEMOBJ_TESTS_BIN
}

function common_setup() {
        if [ -n $DAOS_PATH ]; then
                export LD_LIBRARY_PATH=$DAOS_PATH/prereq/debug/ofi/lib:$LD_LIBRARY_PATH
        fi
        export PMEMOBJ_LOG_LEVEL=10

        PREP_CMD="$TESTS_BIN -S $DIR -f DTX400.a"
        PMEMCHECK_CMD="$TESTS_BIN -S $DIR -f DTX400.b"

        PMREORDER_CMD="./daos_dtx cmd_verify"
}

function common_record() {
        # create a pool and a container first to reduce the size of the recording
        expect_normal_exit $PREP_CMD

        SYS_FILE="$DIR/daos_sys/sys_db"
        POOL_FILE="$DIR/dd6728be-696a-11ef-a059-a4bf0165c389/vpool.0"

        # backup the sys file
        # (the pool file is backed up when the store log is recorded below)
        cp $SYS_FILE $SYS_FILE.pmr

        export PMREORDER_EMIT_LOG=1 # ask PMDK to emit its markers as well
        pmreorder_create_store_log "$POOL_FILE" "$PMEMCHECK_CMD"
        export -n PMREORDER_EMIT_LOG # stop the export

        # restore the sys file
        mv $SYS_FILE.pmr $SYS_FILE

        # Get rid of unnecessary markers just to speed things up.
        # The markers to be removed were hand-picked to match the tested scenario.
        STORE_LOG=store_log$UNITTEST_NUM.log
        tr '|' '\n' < $STORE_LOG > $STORE_LOG.cpy
        grep -v "libpmemobj" $STORE_LOG.cpy | \
                grep -v "libpmem" | \
                grep -v "pmemobj_ctl_set" | \
                grep -v "pmemobj_open" | \
                grep -v "pmemobj_close" | \
                grep -v "pmemobj_root" | \
                grep -v "pmem_memcpy" | \
                grep -v "pmem_memset" > $STORE_LOG.fixed
        tr '\n' '|' < $STORE_LOG.fixed > $STORE_LOG
        rm $STORE_LOG.cpy $STORE_LOG.fixed
}

function common_replay_and_check() {
        # skip reordering and checking stores outside of the markers
        DEFAULT_ENGINE=NoReorderNoCheck
        # XXX The accumulative reordering is sufficient considering the nature of
        # the scenario at hand where the key risk is that not all stores
        # will be executed. The order of these stores is irrelevant.
        # Please see the source code for the details of the tested scenario.
        # Note: ReorderFull is too time-consuming for this scenario.
        EXTENDED_MACROS="PMREORDER_DTX_BASIC=NoReorderDoCheck"

        # Since we do not aim at testing PMDK itself here it is assumed the PMDK
        # APIs just work. No need to reorder or to check its actions step by step.
        # The PMDK APIs on the list were hand-picked to match the APIs used.
        # FUNCS="pmemobj_tx_xalloc pmemobj_tx_add_range_direct pmemobj_tx_xadd_range pmemobj_tx_abort pmemobj_tx_commit"
        # for func in $FUNCS; do
        #         EXTENDED_MACROS="$EXTENDED_MACROS,$func=NoReorderNoCheck"
        # done

        export LD_LIBRARY_PATH="/opt/daos/lib64/daos_srv/:$LD_LIBRARY_PATH"
        pmreorder_expect_success $DEFAULT_ENGINE "$EXTENDED_MACROS" "$PMREORDER_CMD"
}
