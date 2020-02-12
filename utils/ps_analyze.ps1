# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2017, Intel Corporation
#
# ps_analyze -- script to analyze ps1 files
#

Write-Output "Starting PSScript analyzing ..."

$scriptdir = Split-Path -Parent $PSCommandPath
$rootdir = $scriptdir + "\.."
$detected = 0

$include = @("*.ps1" )
Get-ChildItem -Path $rootdir -Recurse -Include $include  | `
    Where-Object { $_.FullName -notlike "*test*" } | `
    ForEach-Object {
        $analyze_result = Invoke-ScriptAnalyzer -Path $_.FullName
        if ($analyze_result) {
            $detected = $detected + $analyze_result.Count
            Write-Output $_.FullName
            Write-Output $analyze_result
        }
    }

if ($detected) {
    Write-Output "PSScriptAnalyzer FAILED. Issues detected: $detected"
    Exit 1
} else {
    Write-Output "PSScriptAnalyzer PASSED. No issue detected."
    Exit 0
}
