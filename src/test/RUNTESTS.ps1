#
# Copyright 2015-2017, Intel Corporation
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
# RUNTESTS -- setup the environment and run each test
#

#
# parameter handling
#
[CmdletBinding(PositionalBinding=$false)]
Param(
    [alias("n")]
    $dryrun = "0",
    [alias("b")]
    $buildtype = "all",
    [alias("t")]
    $testtype = "check",
    [alias("f")]
    $fstype = "all",
    [alias("o")]
    $time = "180s",
    [alias("s")]
    $testfile = "all",
    [alias("i")]
    $testdir = "all",
    [alias("c")]
    $check_pool = "0",
    [alias("k")]
    $skip_dir = "",
    [alias("j")]
    $jobs = 1,
    [alias("h")][switch]
    $help= $false
    )

#
# usage -- print usage message and exit
#
function usage {
    if (1 -eq $args.Count) {
        Write-Host "Error: $args"
    }
    Write-Host "Usage: $0 [ -hnv ] [ -b build-type ] [ -t test-type ] [ -f fs-type ]
                [ -o timeout ] [ -s test-file ] [ -k skip-dir ]
                [ -c ] [ -i testdir ] [-j jobs]
        -h      print this help message
        -n      dry run
        -v      be verbose
        -i test-dir run test(s) from this test directory (default is all)
        -b build-type   run only specified build type
                build-type: debug, nondebug, static-debug, static-nondebug, all (default)
        -t test-type    run only specified test type
                test-type: check (default), short, medium, long, all
                where: check = short + medium; all = short + medium + long
        -k skip-dir skip a specific test directories (for >1 dir enclose in "" and separate with spaces)
        -f fs-type  run tests only on specified file systems
                fs-type: pmem, non-pmem, any, none, all (default)
        -o timeout  set timeout for test execution
                timeout: floating point number with an optional suffix: 's' for seconds
                (the default), 'm' for minutes, 'h' for hours or 'd' for days.
                Default value is 180 seconds.
        -s test-file    run only specified test file
                test-file: all (default), TEST0, TEST1, ...
        -j jobs    number of tests to run simultaneously
        -c      check pool files with pmempool check utility"
    exit 1
}

# -v is a built in PS thing
if ($VerbosePreference -ne 'SilentlyContinue') {
    $verbose = 1
} else {
    $verbose = 0
}

if ($help) {
    usage
    exit 0
}

#
# get_build_dir -- returns the directory to pick the test binaries from
#
# example, to get release build dir
#	get_build_dir "nondebug"
#

function get_build_dir() {

    param([string]$build)

    # default build dir is Debug
    $build_dir = "..\..\x64\Debug"

    if ($build -eq "nondebug") {
        $build_dir = "..\..\x64\Release"
    }

    return $build_dir
}

function read_global_test_configuration {
    if ((Test-Path "config.PS1")) {
        # source the test configuration file
        . ".\config.PS1"
        return;
    }
}

#
# test_concole_output -- print output from test to console
#
function test_concole_output($out, $err) {
    Write-Host -NoNewline $out.Result;
    Write-Host -NoNewline $err.Result;
}

#
# runtest -- given the test directory name, run tests found inside it
#
function runtest {
    $Env:UNITTEST_QUIET = 1
    sv -Name testName $args[0]

    # setup the timeout for seconds (default)
    [int64]$timeval = $time.Substring(0,$time.length-1)
    if ($time -match "m") {
        [int64]$time = $timeval * 60
    } ElseIf ($time -match "h") {
        [int64]$time = $timeval * 60 * 60
    } ElseIf ($time -match "d") {
        [int64]$time = $timeval * 60 * 60 * 24
    } Else {
        [int64]$time = $timeval
    }

    #
    # FS type was forced by user, necessary to treat "any" as either pmem or
    # non-pmem with "-f"
    #
    if($fstype -ne "all") {
        $Env:FORCE_FS= 1
    } else {
        $Env:FORCE_FS= 0
    }

    #
    # make list of fs-types and build-types to test
    #
    sv -Name fss $fstype
    if ($fss -eq "all") {
        $fss = "none pmem non-pmem any"
    }
    sv -Name builds $buildtype
    if ($builds -eq "all") {
        $builds = "debug nondebug"
    }
    if ($skip_dir.split() -contains $testName) {
        Write-Host "RUNTESTS: Skipping: $testName"
        return
    }
    cd $testName
    if ($testfile -eq "all") {
        sv -Name dirCheck ".\TEST*.ps1"
    } else {
        sv -Name dirCheck "..\$testName\$testfile.ps1"
    }
    sv -Name runscripts ""
    Get-ChildItem $dirCheck | Sort-Object { $_.BaseName -replace "\D+" -as [Int] } | % {
        $runscripts += $_.Name + " "
    }

    $runscripts = $runscripts.trim()
    if (-Not($runscripts -match "ps1")) {
        cd ..
        return
    }

    # for each TEST script found...
    Foreach ($runscript in $runscripts.split(" ")) {
        if ($verbose) {
            Write-Host "RUNTESTS: Test: $testName/$runscript "
        }

        read_global_test_configuration
        if ($testtype -ne "all") {
            $type = get-content $runscript | select-string -pattern "require_test_type *" | select -last 1
            if ($type -ne $null) {
                $type = $type.ToString().split(" ")[1];
                if ($testtype -eq "check" -and $type -eq "long") {
                    continue
                }
                if ($testtype -ne "check" -and $testtype -ne $type) {
                    continue
                }
            }
        }

        $test_fss = get-content $runscript | select-string -pattern "require_fs_type *" | select -last 1

        Foreach ($fs in $fss.split(" ").trim()) {
            if ($test_fss -ne $null) {
                $found = 0
                Foreach ($test_fs in $test_fss.ToString().Split(" ") | select -skip 1) {
                    if ($test_fs -eq $fs) {
                        $found = 1
                        break
                    }
                }
                if ($found -eq 0) {
                    continue
                }
            }
            # don't bother trying when fs-type isn't available...
            if ($fs -eq "pmem" -And (-Not $Env:PMEM_FS_DIR)) {
                $pmem_skip = 1
                continue
            }
            if ($fs -eq "non-pmem" -And (-Not $Env:NON_PMEM_FS_DIR)) {
                $non_pmem_skip = 1
                continue
            }
            if ($fs -eq "any" -And (-Not $Env:NON_PMEM_FS_DIR) -And (-Not $Env:PMEM_FS_DIR)) {
                continue
            }

            if ($verbose) {
                Write-Host "RUNTESTS: Testing fs-type: $fs..."
            }
            # for each build-type being tested...
            Foreach ($build in $builds.split(" ").trim()) {
                if ($verbose) {
                    Write-Host "RUNTESTS: Testing build-type: $build..."
                }

                $Env:CHECK_TYPE = $checktype
                $Env:CHECK_POOL = $check_pool
                $Env:VERBOSE = $verbose
                $Env:TYPE = $testtype
                $Env:FS = $fs
                $Env:BUILD = $build
                $Env:EXE_DIR = get_build_dir $build

                if ($dryrun -eq "1") {
                    Write-Host "(in ./$testName) TEST=$testtype FS=$fs BUILD=$build .\$runscript"
                    break
                }

                $sb = {
                    cd $args[0]
                    Invoke-Expression $args[1]
                }
                $j1 = Start-Job -Name $name -ScriptBlock $sb -ArgumentList (pwd).Path, ".\$runscript"

                If ($use_timeout -And $testtype -eq "check") {
                    # execute with timeout
                    $timeout = New-Timespan -Seconds $time
                    $stopwatch = [diagnostics.stopwatch]::StartNew()
                    while (($stopwatch.Elapsed.ToString('hh\:mm\:ss') -lt $timeout) -And `
                        ($j1.State -eq "NotStarted" -or $j1.State -eq "Running")) {
                        Receive-Job -Job $j1
                    }

                    if ($stopwatch.Elapsed.ToString('hh\:mm\:ss') -ge $timeout) {
                        Stop-Job -Job $j1
                        Receive-Job -Job $j1
                        Remove-Job -Job $j1 -Force
                        cd ..
                        throw "RUNTESTS: stopping: $testName/$runscript TIMED OUT, TEST=$testtype FS=$fs BUILD=$build"
                    }
                } Else {
                    Receive-Job -Job $j1 -Wait
                }

                if ($j1.State -ne "Completed") {
                    Remove-Job -Job $j1 -Force
                    cd ..
                    throw "RUNTESTS: stopping: $testName/$runscript FAILED TEST=$testtype FS=$fs BUILD=$build"
                }
                Remove-Job -Job $j1 -Force
            } # for builds
        } # for fss
    } # for runscripts
    cd ..
}

####################

#
# defaults for non-params...
#
sv -Name testconfig ".\testconfig.ps1"
sv -Name use_timeout "ok"
sv -Name checktype "none"

if (-Not (Test-Path "testconfig.ps1")) {
    Write-Error "
RUNTESTS: stopping because no testconfig.ps1 is found.
    to create one:
        cp testconfig.ps1.example testconfig.ps1
    and edit testconfig.ps1 to describe the local machine configuration."
}

. .\testconfig.ps1

if ($Env:TEST_BUILD) {
    $buildtype = $Env:TEST_BUILD
}

if ($Env:TEST_TYPE) {
    $testtype = $Env:TEST_TYPE
}

if ($Env:TEST_FS) {
    $fstype = $Env:TEST_FS
}

if ($Env:TEST_TIMEOUT) {
    $time = $Env:TEST_TIMEOUT
}

if (-Not ("debug nondebug static-debug static-nondebug all" -match $buildtype)) {
    usage "bad build-type: $buildtype"
}
if (-Not ("check short medium long all" -match $testtype)) {
    usage "bad test-type: $testtype"
}
if (-Not ("none pmem non-pmem any all" -match $fstype)) {
    usage "bad fs-type: $fstype"
}

if ($verbose -eq "1") {
    Write-Host -NoNewline "Options:"
    if ($dryrun -eq "1") {
        Write-Host -NoNewline " -n"
    }
    if ($verbose -eq "1") {
        Write-Host -NoNewline " -v"
    }
    Write-Host ""
    Write-Host "    build-type: $buildtype"
    Write-Host "    test-type: $testtype"
    Write-Host "    fs-type: $fstype"
    Write-Host "    check-type: $checktype"
    if ($check_pool -eq "1") {
        sv -Name check_pool_str "yes"
    } else {
        sv -Name check_pool_str "no"
    }
    Write-Host "    check-pool: $check_pool_str"
    Write-Host "Tests: $args"
}

$threads = 0
$status = 0

try {
    if ($testdir -eq "all") {
        if ($jobs -gt 1) {
            $it = 0
            # unique name for all jobs
            $name = [guid]::NewGuid().ToString()
            $tests = (Get-ChildItem -Directory)

            # script block - job's start function
            $sb = {
                cd $args[0]
                $LASTEXITCODE = 0
                .\RUNTESTS.ps1 -dryrun $args[1] -buildtype $args[2]
                        -testtype $args[3] -fstype $args[4] -time $args[5]
                        -testdir $args[6]
                if ($LASTEXITCODE -ne 0) {
                    throw "RUNTESTS FAILED $args[0]"
                }
            }

            # start worker jobs
            1..$jobs | % {
                if ($it -lt $tests.Length) {
                    $j1 = Start-Job -Name $name -ScriptBlock $sb
                            -ArgumentList (pwd).ToString(),
                            $dryrun, $buildtype, $testtype, $fstype,
                            $time, $tests[$it].Name | Out-Null
                    $it++
                    $threads++
                }
            }

            $fail = $false

            # control loop for receiving job outputs and starting new jobs
            while ($threads -ne 0) {
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
                            Start-Job -Name $name -ScriptBlock $sb
                                    -ArgumentList (pwd).ToString(),
                                    $dryrun, $buildtype, $testtype, $fstype,
                                    $time, $tests[$it].Name | Out-Null
                            $it++
                            $threads++
                        }
                    }
                }
            }
            if ($fail -eq $true) {
                Write-Error "RUNTESTS FAILED"
                Exit 1
            }
        } else {
            # only one job
            Get-ChildItem -Directory | % {
                $LASTEXITCODE = 0
                runtest $_.Name
            }
        }
    } else {
        # only one test
        $LASTEXITCODE = 0
        runtest $testdir
    }
} catch {
    Write-Error "RUNTESTS FAILED"
    $status = 1
} finally {
    # cleanup jobs in case of exception or C-c
    if ($threads -ne 0) {
        Get-Job -name $name | Remove-Job -Force
    }
}

Exit $status
