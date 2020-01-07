#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2018, Intel Corporation
#
#
# rpmem_basic/config.sh -- test configuration
#

CONF_GLOBAL_FS_TYPE=any
CONF_GLOBAL_BUILD_TYPE="debug nondebug"

CONF_GLOBAL_RPMEM_PROVIDER=all
CONF_GLOBAL_RPMEM_PMETHOD=all

CONF_RPMEM_PMETHOD[10]=APM
CONF_RPMEM_PMETHOD[11]=GPSPM

# Sockets provider does not detect fi_cq_signal so it does not return
# from fi_cq_sread. It causes this test to hang sporadically.
# https://github.com/ofiwg/libfabric/pull/3645
CONF_RPMEM_PROVIDER[12]=verbs
