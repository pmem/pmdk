#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2024, Intel Corporation

#
# src/test/ex_libpmemobj/common.sh -- common setup of unit test
# for libpmemobj examples
#

LOG=out$UNITTEST_NUM.log

LOG_TEMP=out${UNITTEST_NUM}_part.log
rm -f $LOG_TEMP && touch $LOG_TEMP
