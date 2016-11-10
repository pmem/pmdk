#
# Copyright 2015-2016, Intel Corporation
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
    [alias("m")]
    $mreceivetype = "auto",
    [alias("p")]
    $preceivetype = "auto",
    [alias("d")]
    $dreceivetype = "auto",
    [alias("o")]
    $time = "60s",
    [alias("s")]
    $testfile = "all",
    [alias("i")]
    $testdir = "all",
    [alias("c")]
    $check_pool = "0",
    [alias("h")][switch]
    $help= $false
    )

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
# usage -- print usage message and exit
#
function usage {
    if (1 -eq $args.Count) {
        Write-Host "Error: $args"
    }
    Write-Host "Usage: $0 [ -hnv ] [ -b build-type ] [ -t test-type ] [ -f fs-type ]
                [ -o timeout ] [ -s test-file ] [ -m memcheck ] [-p pmemcheck ] [ -e helgrind ] [ -d drd ] [ -c ] [ -i testdir ]
        -h      print this help message
        -n      dry run
        -v      be verbose
        -i test-dir run test(s) from this test directory (default is all)
        -b build-type   run only specified build type
                build-type: debug, nondebug, static-debug, static-nondebug, all (default)
        -t test-type    run only specified test type
                test-type: check (default), short, medium, long, all
                where: check = short + medium; all = short + medium + long
        -f fs-type  run tests only on specified file systems
                fs-type: pmem, non-pmem, any, none, all (default)
        -o timeout  set timeout for test execution
                timeout: floating point number with an optional suffix: 's' for seconds
                (the default), 'm' for minutes, 'h' for hours or 'd' for days.
                Default value is 60 seconds.
        -s test-file    run only specified test file
                test-file: all (default), TEST0, TEST1, ...
        -m memcheck run tests with memcheck
                memcheck: auto (default, enable/disable based on test requirements),
                force-enable (enable when test does not require memcheck, but
                obey test's explicit memcheck disable)
        -p pmemcheck    run tests with pmemcheck
                pmemcheck: auto (default, enable/disable based on test requirements),
                force-enable (enable when test does not require pmemcheck, but
                obey test's explicit pmemcheck disable)
        -e helgrind run tests with helgrind
                helgrind: auto (default, enable/disable based on test requirements),
                force-enable (enable when test does not require helgrind, but
                obey test's explicit helgrind disable)
        -d drd      run tests with drd
                drd: auto (default, enable/disable based on test requirements),
                force-enable (enable when test does not require drd, but
                obey test's explicit drd disable)
        -c      check pool files with pmempool check utility"
    exit 1
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


if (-Not ("debug nondebug static-debug static-nondebug all" -match $buildtype)) {
    usage "bad build-type: $buildtype"
}
if (-Not ("check short long" -match $testtype)) {
    usage "bad test-type: $testtype"
}
if (-Not ("none pmem non-pmem any all" -match $fstype)) {
    usage "bad fs-type: $fstype"
}

# XXX :missing some logic here that's in the bash script
# having to do with force-enable and memcheck, pmemcheck.
# don't really get whats going on there but we don't support
# either in windows right now so will just save the params
if (-Not ("auto" -match $mreceivetype)) {
    usage "bad memcheck: $mreceivetype"
}
if (-Not ("auto" -match $preceivetype)) {
    usage "bad pmemcheck: $preceivetype"
}
if (-Not ("auto" -match $ereceivetype)) {
    usage "bad helgrind: $ereceivetype"
}
if (-Not ("auto" -match $dreceivetype)) {
    usage "bad drd: $dreceivetype"
}
sv -Name receivetype $mreceivetype

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
    } ElseIf ($time -eq "h") {
        [int64]$time = $timeval * 60 * 60
    } ElseIf ($time -eq "d") {
        [int64]$time = $timeval * 60 * 60 * 24
    } Else {
        [int64]$time = $timeval
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

    Foreach ($fs in $fss.split(" ").trim()) {
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
            $Env:TEST = $testtype
            $Env:FS = $fs
            $Env:BUILD = $build
            $Env:EXE_DIR = get_build_dir $build

            $pinfo = New-Object System.Diagnostics.ProcessStartInfo
            $pinfo.FileName = "powershell.exe"
            $pinfo.RedirectStandardError = $true
            $pinfo.RedirectStandardOutput = $true
            $pinfo.UseShellExecute = $false

            # for each TEST script found...
            Foreach ($runscript in $runscripts.split(" ")) {
                if ($verbose) {
                    Write-Host -NoNewline "RUNTESTS: Test: $testName/$runscript "
                }
                if ($dryrun -eq "1") {
                    Write-Host "(in ./$testName) TEST=$testtype FS=$fs BUILD=$build .\$runscript"
                    break
                }
                $pinfo.Arguments = ".\$runscript"
                $pinfo.WorkingDirectory = $(pwd).Path
                $p = New-Object System.Diagnostics.Process
                $p.StartInfo = $pinfo
                $p.Start() | Out-Null
                If ($use_timeout -And $testtype -eq "check") {
                    # execute with timeout
                    $timeout = new-timespan -Seconds $time
                    $stopwatch = [diagnostics.stopwatch]::StartNew()
                    while (($stopwatch.elapsed -lt $timeout) -And `
                        ($p.HasExited -eq $false)) {
                        # output streams have limited size, we need to read it
                        # during an application runtime to prevent application hang.
                        Write-Host -NoNewline $p.StandardOutput.ReadToEnd();
                        Write-Host -NoNewline $p.StandardError.ReadToEnd();
                    }
                    if ($stopwatch.elapsed -ge $timeout) {
                        $p | Stop-Process -Force
                        Write-Error "RUNTESTS: stopping: $testName/$runscript TIMED OUT, TEST=$testtype FS=$fs BUILD=$build"
                        cd ..
                    }
                } Else {
                    $p.WaitForExit()
                }

                # print any remaining output
                Write-Host -NoNewline $p.StandardOutput.ReadToEnd();
                Write-Host -NoNewline $p.StandardError.ReadToEnd();
                if ($p.ExitCode -ne 0) {
                    Write-Error "RUNTESTS: stopping: $testName/$runscript $msg errorcde= $p.ExitCode, TEST=$testtype FS=$fs BUILD=$build"
                    cd ..
                    exit $p.ExitCode
                }
            } # for runscripts
        } # for builds
    } # for fss
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
if ($testdir -eq "all") {
    Get-ChildItem -Directory | % {
        $LASTEXITCODE = 0
        runtest $_.Name
        if ($LASTEXITCODE -ne 0) {
            Write-Host ""
            Write-Error "RUNTESTS FAILED at $test_script"
            Exit $LASTEXITCODE
        }
    }
} else {
    $LASTEXITCODE = 0
    runtest $testdir
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Error "RUNTESTS FAILED at $test_script"
        Exit $LASTEXITCODE
    }
}
