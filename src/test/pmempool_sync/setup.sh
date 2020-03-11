#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020, IBM Corporation
#

POOLSIZE=$(($(getconf PAGESIZE)/1024*2))
POOLSIZE_REP=$(($POOLSIZE*3))
