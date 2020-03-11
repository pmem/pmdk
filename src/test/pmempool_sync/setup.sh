#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, IBM Corporation
#

PARTSIZE=$(($(getconf PAGESIZE)/1024*2))
POOLSIZE_REP=$(($PARTSIZE*3))
