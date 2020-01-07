# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2016-2017, Intel Corporation
#
# CSTYLE.ps1 -- script to check coding style
#
# XXX - integrate with VS projects and execute for each build
#

$scriptdir = Split-Path -Parent $PSCommandPath
$rootdir = $scriptdir + "\.."
$cstyle = $rootdir + "\utils\cstyle"
$checkdir = $rootdir

# XXX - *.cpp/*.hpp files not supported yet
$include = @( "*.c", "*.h" )

If ( Get-Command -Name perl -ErrorAction SilentlyContinue ) {
	Get-ChildItem -Path $checkdir -Recurse -Include $include | `
    Where-Object { $_.FullName -notlike "*jemalloc*" } | `
    ForEach-Object {
        $IGNORE = $_.DirectoryName + "\.cstyleignore"
        if(Test-Path $IGNORE) {
            if((Select-String $_.Name $IGNORE)) {
                return
            }
        }
        $_
    } | ForEach-Object {
		Write-Output $_.FullName
		& perl $cstyle $_.FullName
		if ($LASTEXITCODE -ne 0) {
            Exit $LASTEXITCODE
        }
    }
} else {
	Write-Output "Cannot execute cstyle - perl is missing"
}
