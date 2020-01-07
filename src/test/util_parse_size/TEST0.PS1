# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2015-2019, Intel Corporation

#
# src/test/util_parse_size/TEST0 -- unit test for util_parse_size.
#

. ..\unittest\unittest.ps1

require_test_type medium

require_fs_type none

setup

expect_normal_exit $Env:EXE_DIR\util_parse_size$Env:EXESUFFIX 11K 11M 11G 11T 11P `
		11KiB 11MiB 11GiB 11TiB 11PiB 11kB 11MB 11GB 11TB 11PB 1234 `
		10k 10KB 10mB 10mb 10Mb 10B B 10ki 10KiC KiD `
		10Kiboli 10Kboli 10boli 10KiBoli

check

pass
