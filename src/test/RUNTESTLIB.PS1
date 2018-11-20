#
# Copyright 2015-2018, Intel Corporation
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
# RUNTESTSLIB.PS1 -- functions used in runtest script
#

class Config {
    [bool] $dryrun
    [String] $buildtype
    [String] $testtype
    [String] $fstype
    [timespan] $timeout
    [string]$testfile
    [bool] $check_pool
    [string] $skip_dir
    [Object[]] $testdir
    [bool] $checkfs
    [bool] $forcefs
    [string] $verbose

    #
    # setTimeout -- parse timeout
    #
    setTimeout([string]$time) {
        if($time -match "^\d+$") {
            $this.timeout = [timespan]::FromSeconds($time)
            return
        }

        [int64]$timeval = $time.Substring(0,$time.length-1)
        if ($time -match "m") {
            $this.timeout = [timespan]::FromMinutes($timeval)
        } elseif ($time -match "h") {
            $this.timeout = [timespan]::FromHours($timeval)
        } elseif ($time -match "d") {
            $this.timeout = [timespan]::FromDays($timeval)
        } else {
            $this.timeout = [timespan]::FromSeconds($timeval)
        }
    }

    #
    # setFstype -- parse fs-type
    #
    setFstype([string]$fstype) {
        $fs ="none"
        # don't bother trying when fs-type isn't available...
        if ($Env:PMEM_FS_DIR) {
            $fs += " pmem"
        }

        if ($Env:NON_PMEM_FS_DIR) {
            $fs += " non-pmem"
        }

        if ($Env:NON_PMEM_FS_DIR -or $Env:PMEM_FS_DIR) {
            $fs += " any"
        }

        if ($fstype -ne "all") {
            #
            # FS type was forced by user, necessary to treat
            # "any" as either pmem or non-pmem with "-f"
            #
            $this.forcefs = $true
            $this.fstype = $fstype
        } else {
            $this.fstype = $fs
            $this.forcefs = $false
        }

    }

    #
    # setBuildtype -- parse build type
    #
    setBuildtype([string]$buildtype) {
        if ($buildtype -eq "all") {
            $this.buildtype = "debug nondebug"
        } else {
            $this.buildtype = $buildtype
        }
    }

    #
    # setTestdir -- parse test directory
    #
    setTestdir($testdir) {
        if ($testdir -eq "all") {
            $this.testdir = Get-ChildItem -Directory
        } else {
            $this.testdir = Get-Item $testdir
        }
    }
}

#
# usage -- print usage message and exit
#
function usage {
Param (
   [parameter(Position=0)]
   [ValidateNotNullOrEmpty()]
   [String]$name = $(throw "Missing application name")
)

Write-Host "Usage: $name [ -hnv ] [ -b build-type ] [ -t test-type ] [ -f fs-type ]
            [ -o timeout ] [ -s test-file ] [ -k skip-dir ]
            [ -c ] [ -i testdir ] [-j jobs]
    -h      print this help message
    -n      dry run
    -v      be verbose
    -i test-dir run test(s) from this test directory (default is all)
    -b build-type   run only specified build type
            build-type: debug, nondebug, all (default)
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

#
# get_build_dirs -- returns the directories to pick the test binaries from
#
# example, to get release build dirs
#	get_build_dirs "nondebug"
#
$DEBUG_DIR = '..\..\x64\Debug'
$RELEASE_DIR = '..\..\x64\Release'

function get_build_dirs() {
    param(
        [ValidateSet("debug", "nondebug")]
        [string]$build
    )
	$build_dirs = @()

    if ($build -eq "debug") {
		$build_dirs += $DEBUG_DIR + "\tests"
		$build_dirs += $DEBUG_DIR + "\examples"
		$build_dirs += $DEBUG_DIR + "\libs"
	} else {
        $build_dirs += $RELEASE_DIR + "\tests"
		$build_dirs += $RELEASE_DIR + "\examples"
		$build_dirs += $RELEASE_DIR + "\libs"
    }

    return $build_dirs
}

#
# read_global_test_configuration -- load per test configuration
#
function read_global_test_configuration {
    if ((Test-Path "config.PS1")) {
        # source the test configuration file
        . ".\config.PS1"
        return;
    }
}

#
# save_env_variables -- save environment variables
#
function save_env_variables {

    $old = New-Object System.Collections.ArrayList
    $old.AddRange((Get-ChildItem Env:))

    return $old
}

#
# restore_env_variables -- restore environment variables
#
function restore_env_variables {
    param([System.Collections.ArrayList]$old)

    $new = New-Object System.Collections.ArrayList
    $new.AddRange((Get-ChildItem Env:))

    $old.ToArray() | foreach {
        if($new.contains($_)) {
            $old.Remove($_)
            $new.Remove($_)
        }
    }

    $new.ToArray() | foreach {
        [Environment]::SetEnvironmentVariable($_.Key, $null)
    }
    $old.ToArray() | foreach {
        [Environment]::SetEnvironmentVariable($_.Key, $_.Value)
    }

}

#
# runtest -- given the test directory name, run tests found inside it
#
function runtest {
    param (
    [Parameter(ValueFromPipeline = $true, Mandatory = $true)]
    [string] $testName,

    [Parameter(Mandatory = $true)]
    [Config] $config
    )

    # reset environment variables which can affect test execution
    $Env:UNITTEST_NAME=$null

    if (-Not $Env:UNITTEST_LOG_LEVEL) {
        $Env:UNITTEST_LOG_LEVEL = 1
    }

    if ($config.forcefs) {
        $Env:FORCE_FS= 1
    } else {
        $Env:FORCE_FS= 0
    }

    if ($config.testfile -eq "all") {
        $dirCheck = ".\TEST*.ps1"
    } else {
        $dirCheck = ".\" + $config.testfile + ".ps1"
    }

    $runscripts = ""
    Get-ChildItem $dirCheck | Sort-Object { $_.BaseName -replace "\D+" -as [Int] } | % {
        $runscripts += $_.Name + " "
    }

    $runscripts = $runscripts.trim()

    if (-not $runscripts) {
        return
    }

    # for each TEST script found...
    Foreach ($runscript in $runscripts.split(" ")) {
        Write-Verbose "RUNTESTS: Test: $testName/$runscript "

        read_global_test_configuration

        Foreach ($fs in $config.fstype.split(" ").trim()) {
            Write-Verbose "RUNTESTS: Testing fs-type: $fs..."
            # for each build-type being tested...
            Foreach ($build in $config.buildtype.split(" ").trim()) {
                Write-Verbose "RUNTESTS: Testing build-type: $build..."

                $Env:CHECK_POOL = $config.check_pool
                $Env:TYPE = $config.testtype
                $Env:FS = $fs
                $Env:BUILD = $build
                $Env:EXE_DIR = $(get_build_dirs $build)[0]
                $Env:EXAMPLES_DIR = $(get_build_dirs $build)[1]

                if ($Env:BUILD -eq 'nondebug') {
                    if (-Not $Env:PMDK_LIB_PATH_NONDEBUG) {
                        $Env:LIBS_DIR = $(get_build_dirs $build)[2]
                    } else {
                        $Env:LIBS_DIR = $Env:PMDK_LIB_PATH_NONDEBUG
                    }
                } elseif ($Env:BUILD -eq 'debug') {
                    if (-Not $Env:PMDK_LIB_PATH_DEBUG) {
                        $Env:LIBS_DIR = $(get_build_dirs $build)[2]
                    } else {
                        $Env:LIBS_DIR = $Env:PMDK_LIB_PATH_DEBUG
                    }
                }

                if ($dryrun -eq "1") {
                    Write-Host "(in ./$testName) TEST=$testtype FS=$fs BUILD=$build .\$runscript"
                    continue
                }

                $save = save_env_variables

                # run test
                Invoke-Expression ./$runscript
                $ret = $?

                # reset timeout timer
                New-Event -SourceIdentifier "timeout-reset" | out-null

                restore_env_variables($save)

                if (-not $ret) {
                    throw "RUNTESTS: stopping: $testName/$runscript FAILED TEST=$testtype FS=$fs BUILD=$build"
                }
            } # for builds
        } # for fss
    } # for runscripts
}

function isDir {
    if (-Not $args[0]) {
        return $false
    }
    return Test-Path $args[0] -PathType Container
}

if (-Not (Test-Path "./testconfig.ps1")) {
    throw $MyInvocation.MyCommand.Name + " stopping because no testconfig.ps1 is found.
To create one:
    Copy-Item testconfig.ps1.example testconfig.ps1
and edit testconfig.ps1 to describe the local machine configuration.
"
}

# need to manually clear variables, if definition of a variable was removed
# from testconfig RUNTEST would use previously exported value
$Env:PMEM_FS_DIR=""
$Env:NON_PMEM_FS_DIR=""
$Env:PMEM_IS_PMEM_FORCE=$null

. .\testconfig.ps1

if ($Env:PMEM_FS_DIR -And (-Not (isDir($Env:PMEM_FS_DIR)))) {
    throw "error: PMEM_FS_DIR=$Env:PMEM_FS_DIR doesn't exist"
}

if ($Env:NON_PMEM_FS_DIR -And (-Not (isDir($Env:NON_PMEM_FS_DIR)))) {
    throw "error: NON_PMEM_FS_DIR=$Env:NON_PMEM_FS_DIR doesn't exist"
}

# run this code in the separate thread to not pollute PATH variable in user shell.
Start-Job -Name $name -Args $PSScriptRoot -ScriptBlock {
    param ([string]$dir)
    cd $dir

    # in script block there is no access to other functions
    function isDir {
        if (-Not $args[0]) {
            return $false
        }
        return Test-Path $args[0] -PathType Container
    }

    $PMEMDETECT="..\x64\Debug\tests\pmemdetect.exe"

    if (-Not (Test-Path $PMEMDETECT)) {
        $PMEMDETECT="..\x64\Release\tests\pmemdetect.exe"
        $Env:Path =  "..\x64\Release\libs;" + $Env:Path
    } else {
        $Env:Path =  "..\x64\Debug\libs;" + $Env:Path
    }


    if (isDir($Env:PMEM_FS_DIR)) {
        if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
            # "0" means there is PMEM
            $Local:PMEM_IS_PMEM = "0"
        } else {
            &$PMEMDETECT $Env:PMEM_FS_DIR
            $Local:PMEM_IS_PMEM = $Global:LASTEXITCODE
        }

        if ($Local:PMEM_IS_PMEM -ne "0") {
            throw "error: PMEM_FS_DIR=$Env:PMEM_FS_DIR does not point to a PMEM device"
        }
    }

    if (isDir($Env:NON_PMEM_FS_DIR)) {
        &$PMEMDETECT $Env:NON_PMEM_FS_DIR
        $Local:NON_PMEM_IS_PMEM = $Global:LASTEXITCODE

        if ($Local:NON_PMEM_IS_PMEM -eq "0") {
            throw "error: NON_PMEM_FS_DIR=$Env:NON_PMEM_FS_DIR does not point to a non-PMEM device"
        }
    }
} | Receive-Job -Wait -AutoRemoveJob
