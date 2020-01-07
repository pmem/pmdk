# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2017, Intel Corporation
#
# CHECK_WHITESPACE.PS1 -- script to check coding style
#
# XXX - integrate with VS projects and execute for each build
#

$scriptdir = Split-Path -Parent $PSCommandPath
$rootdir = $scriptdir + "\.."
$whitepace = $rootdir + "\utils\check_whitespace"

If ( Get-Command -Name perl -ErrorAction SilentlyContinue ) {
	&perl $whitepace -g
	if ($LASTEXITCODE -ne 0) {
			Exit $LASTEXITCODE
	}
} else {
	Write-Output "Cannot execute check_whitespace - perl is missing"
}
