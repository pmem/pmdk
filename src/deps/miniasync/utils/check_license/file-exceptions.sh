#!/bin/sh -e
# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2020, Intel Corporation

# file-exceptions.sh - filter out files not checked for copyright and license

grep -v -E -e 'src/non-existing-path-to-be-replaced-with-real-one'
