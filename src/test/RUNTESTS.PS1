#
# Copyright 2015-2019, Intel Corporation
# Copyright (c) 2016, Microsoft Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#
#     * Neither the name of the copyright holder nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# RUNTESTS.PS1 -- setup the environment and run each test
#

#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("n")]
    [switch]$dryrun = $false,

    [alias("b")]
    [ValidateSet("all", "debug", "release")]
    [string]$buildtype = "all",

    [alias("t")]
    [ValidateSet("check", "short", "medium", "long", "all")]
    [string]$testtype = "check",

    [alias("f")]
    [ValidateSet("pmem", "non-pmem", "any", "none", "all")]
    [string]$fstype = "all",

    [alias("o")]
    [ValidateScript({
        if( $_ -match "^\d+[smhd]?$") {
            $true
        } else {
            throw "$_ is not valid timeout value."
        }
    })]
    [string]$time = "180s",

    [alias("s")]
    [string]$testfile = "all",

    [alias("i")]
    [ValidateScript({
        if($_ -eq "all") {
            $true
        } elseif(Test-Path -Path $_ -pathType container)  {
            $true
        } else {
            throw "Directory $_ doesn't exist."
        }
    })]
    [string]$testdir = "all",

    [alias("c")]
    [switch]$check_pool = $false,

    [alias("k")]
    [string]$skip_dir = "",

    [alias("j")]
    [uint32]$jobs = 1,

    [alias("h")]
    [switch]$help= $false
)

if ($PSVersionTable.PSVersion.Major -lt 5) {
    throw $MyInvocation.MyCommand.Name + " require powershell version >= 5"
}

. .\RUNTESTLIB.PS1


if($help) {
    usage $MyInvocation.MyCommand.Name
}

Write-Verbose "Options: -v $(if ($dryrun) {"-n"})"
Write-Verbose "    build-type: $buildtype"
Write-Verbose "    test-type: $testtype"
Write-Verbose "    fs-type: $fstype"
Write-Verbose "    check-pool: $(if ($check_pool -eq "1") {"yes"} else {"no"})"

$config = New-Object Config


$config.setBuildtype($buildtype)
$config.setFstype($fstype)
$config.setTimeout($time)
$config.setTestdir($testdir)
$config.testtype = $testtype
$config.check_pool = $check_pool
$config.skip_dir = $skip_dir
$config.testfile = $testfile
$config.verbose = $VerbosePreference

if ($config.testtype -eq "check") {
    Register-EngineEvent -SourceIdentifier "timeout-reset" -Action {
        $Global:stopwatch = [diagnostics.stopwatch]::StartNew()
    } | Out-Null
    $Global:stopwatch = [diagnostics.stopwatch]::StartNew()
} else {
    # disable timeout
    $config.setTimeout(0)
    $Global:stopwatch = [diagnostics.stopwatch]::StartNew()
    $Global:stopwatch.Stop()
}
# script blocks - job's start functions

$sb_ST = {
    param ([string]$dir, $config) # Config type is unknown here
    $VerbosePreference = $config.verbose
    Register-EngineEvent -SourceIdentifier "timeout-reset" -Forward # catch event and forward it to parent job
    cd $dir
    . .\RUNTESTLIB.PS1

    $config.testdir | % {
        if ($config.skip_dir.split() -contains $_) {
            Write-Host "RUNTESTS: Skipping: $testName"
            return
        }

        cd $_
        $_ | runtest -config $config
        cd ..
    }
}

$sb_MT = {
    param ([string]$dir, $config, [string]$test) # Config type is unknown here
    $VerbosePreference = $config.verbose
    Register-EngineEvent -SourceIdentifier "timeout-reset" -Forward # catch event and forward it to parent job
    cd $dir
    . .\RUNTESTLIB.PS1

    if ($config.skip_dir.split() -contains $_) {
        Write-Host "RUNTESTS: Skipping: $testName"
        return
    }
    cd $test
    $test | runtest -config $config
    cd ..
}

# unique name for all jobs
$name = [guid]::NewGuid().ToString()

try {
    if ($jobs -gt 1) {
        $it = 0
        $threads = 0
        $tests = $config.testdir

        # start worker jobs
        1..$jobs | % {
            if ($it -lt $tests.Length) {
                Start-Job -Name $name -Args $PSScriptRoot, $config, $tests[$it] -ScriptBlock $sb_MT | Out-Null
                $it++
                $threads++
            }
        }

        $fail = $false

        # control loop for receiving job outputs and starting new jobs
        while ($threads -ne 0) {
            if ($config.timeout.TotalSeconds -ne 0 -and $Global:stopwatch.Elapsed.TotalSeconds -ge $config.timeout.TotalSeconds) {
                Get-Job -name $name | Remove-Job -Force
                throw "RUNTESTS: stopping: TIMED OUT"
            }
            Get-Job -name $name | Receive-Job
            Get-Job -name $name | % {
                if ($_.State -eq "Running" -or $_.State -eq "NotStarted") {
                    return
                }
                if ($_.State -eq "Failed") {
                    $fail = $true
                }
                Receive-Job $_
                Remove-Job $_ -Force
                $threads--
                if ($fail -eq $false) {
                    if ($it -lt $tests.Length) {
                        Start-Job -Name $name -Args $PSScriptRoot, $config, $tests[$it] -ScriptBlock $sb_MT | Out-Null
                        $it++
                        $threads++
                    }
                }
            }
        }
        if ($fail -eq $true) {
            throw "one of the tests failed"
        }
    } else {
        # if there is no timeout don't run tests in separate thread - useful for script debugging
        if ($config.timeout.TotalSeconds -eq 0) {
            & $sb_ST $PSScriptRoot $config
        } else {
            $job = Start-Job -Name $name -Args $PSScriptRoot, $config -ScriptBlock $sb_ST -Verbose
            $threads++
            while ($Global:stopwatch.Elapsed.TotalSeconds -lt $config.timeout.TotalSeconds -and  $(Get-Job).ChildJobs.Count -ne 0) {
                Receive-Job -Job $job

                if ($job.State -eq "Running" -or $job.State -eq "NotStarted") {
                    sleep -Milliseconds 100
                    continue
                }
                if ($job.State -eq "Failed") {
                    Remove-Job -job $job -Force | out-null
                    $threads--
                    throw "one of the tests failed"
                }

                Receive-Job $job
                Remove-Job $job -Force
                $threads--
                return
            }

            if ($Global:stopwatch.Elapsed.TotalSeconds -ge $config.timeout.TotalSeconds) {
                Receive-Job -Job $job
                throw "TIMED OUT"
            }
        }
    }
} catch {
    # in case of fail test without timeout configured
    # we have to return to src/test dir
    if ($config.timeout.TotalSeconds -eq 0) {
        cd ..
    }
    Write-Error "RUNTESTS FAILED: $_"
    $status = 1
} finally {
    # cleanup jobs in case of exception or C-c
    if ($config.timeout.TotalSeconds -ne 0) {
        Get-Job -name "timeout-reset"| Remove-Job -Force
    }
    if ($threads -gt 0) {
        Get-Job -name $name | Remove-Job -Force
    }
}

Exit $status
