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

. "..\testconfig.ps1"

function verbose_msg {
    if ($Env:UNITTEST_LOG_LEVEL -ge "2") {
        Write-Host $args[0]
    }
}

function msg {
    if ($Env:UNITTEST_LOG_LEVEL -ge "1") {
        Write-Host $args[0]
    }
}

function fatal {
    throw $args[0]
}

function touch {
    out-file -InputObject $null -Encoding ascii -literalpath $args[0]
}

function epoch {
    return [int64](([datetime]::UtcNow)-(get-date "1/1/1970")).TotalMilliseconds
}

function isDir {
    if (-Not $args[0]) {
        return $false
    }
    return Test-Path $args[0] -PathType Container
}

# force dir w/wildcard to fail if no match
function dirFailOnEmpty {
    if (0 -eq (Get-ChildItem $args[0]).Count) {
        throw -Message 'No match: $args[0]'
    }
}

function getLineCount {
    [int64]$numLines = 0
    $buff = New-Object IO.StreamReader $args[0]
    while ($buff.ReadLine() -ne $null){ $numLines++ }
    $buff.Close()
    return $numLines
}

#
# convert_to_bytes -- converts the string with K, M, G or T suffixes
# to bytes
#
# example:
#   "1G" --> "1073741824"
#   "2T" --> "2199023255552"
#   "3k" --> "3072"
#   "1K" --> "1024"
#   "10" --> "10"
#
function convert_to_bytes() {

    param([string]$size)

    if ($size.ToLower().EndsWith("kib")) {
        $size = [int64]($size.Substring(0, $size.Length - 3)) * 1kb
    } elseif ($size.ToLower().EndsWith("mib")) {
        $size = [int64]($size.Substring(0, $size.Length - 3)) * 1mb
    } elseif ($size.ToLower().EndsWith("gib")) {
        $size = [int64]($size.Substring(0, $size.Length - 3)) * 1gb
    } elseif ($size.ToLower().EndsWith("tib")) {
        $size = [int64]($size.Substring(0, $size.Length - 3)) * 1tb
    } elseif ($size.ToLower().EndsWith("pib")) {
        $size = [int64]($size.Substring(0, $size.Length - 3)) * 1pb
    } elseif ($size.ToLower().EndsWith("kb")) {
        $size = [int64]($size.Substring(0, $size.Length - 2)) * 1000
    } elseif ($size.ToLower().EndsWith("mb")) {
        $size = [int64]($size.Substring(0, $size.Length - 2)) * 1000 * 1000
    } elseif ($size.ToLower().EndsWith("gb")) {
        $size = [int64]($size.Substring(0, $size.Length - 2)) * 1000 * 1000 * 1000
    } elseif ($size.ToLower().EndsWith("tb")) {
        $size = [int64]($size.Substring(0, $size.Length - 2)) * 1000 * 1000 * 1000 * 1000
    } elseif ($size.ToLower().EndsWith("pb")) {
        $size = [int64]($size.Substring(0, $size.Length - 2)) * 1000 * 1000 * 1000 * 1000 * 1000
    } elseif ($size.ToLower().EndsWith("b")) {
        $size = [int64]($size.Substring(0, $size.Length - 1))
    } elseif ($size.ToLower().EndsWith("k")) {
        $size = [int64]($size.Substring(0, $size.Length - 1)) * 1kb
    } elseif ($size.ToLower().EndsWith("m")) {
        $size = [int64]($size.Substring(0, $size.Length - 1)) * 1mb
    } elseif ($size.ToLower().EndsWith("g")) {
        $size = [int64]($size.Substring(0, $size.Length - 1)) * 1gb
    } elseif ($size.ToLower().EndsWith("t")) {
        $size = [int64]($size.Substring(0, $size.Length - 1)) * 1tb
    } elseif ($size.ToLower().EndsWith("p")) {
        $size = [int64]($size.Substring(0, $size.Length - 1)) * 1pb
    } elseif (($size -match "^[0-9]*$") -and ([int64]$size -gt 1023)) {
        #
        # Because powershell converts 1kb to 1024, and we convert it to 1000, we
        # catch byte values greater than 1023 to be suspicious that caller might
        # not be aware of the silent conversion by powershell.  If the caller
        # knows what she is doing, she can always append 'b' to the number.
        #
        fatal "Error suspicious byte value to convert_to_bytes"
    }

    return [Int64]$size
}

#
# truncate -- shrink or extend a file to the specified size
#
# A file that does not exist is created (holey).
#
# XXX: Modify/rename 'sparsefile' to make it work as Linux 'truncate'.
# Then, this cmdlet is not needed anymore.
#
function truncate {
    [CmdletBinding(PositionalBinding=$true)]
    Param(
        [alias("s")][Parameter(Mandatory = $true)][string]$size,
        [Parameter(Mandatory = $true)][string]$fname
    )

    [int64]$size_in_bytes = (convert_to_bytes $size)

    if (-Not (Test-Path $fname)) {
        & $SPARSEFILE $fname $size_in_bytes 2>&1 1>> $Env:PREP_LOG_FILE

    } else {
        $file = new-object System.IO.FileStream $fname, Open, ReadWrite
        $file.SetLength($size_in_bytes)
        $file.Close()
    }
}

#
# create_file -- create zeroed out files of a given length
#
# example, to create two files, each 1GB in size:
#	create_file 1G testfile1 testfile2
#
# Note: this literally fills the file with 0's to make sure its
# not a sparse file.  Its slow but the fastest method I could find
#
# Input unit size is in bytes with optional suffixes like k, KB, M, etc.
#
function create_file {
    [int64]$size = (convert_to_bytes $args[0])
    for ($i=1;$i -lt $args.count;$i++) {
        $stream = new-object system.IO.StreamWriter($args[$i], "False", [System.Text.Encoding]::Ascii)
        1..$size | %{ $stream.Write("0") }
        $stream.close()
        Get-ChildItem $args[$i]* >> $Env:PREP_LOG_FILE
    }
}

#
# create_holey_file -- create holey files of a given length
#
# example:
#	create_holey_file 1024k testfile1 testfile2
#	create_holey_file 2048M testfile1 testfile2
#	create_holey_file 234 testfile1
#	create_holey_file 2340b testfile1
#
# Input unit size is in bytes with optional suffixes like k, KB, M, etc.
#
function create_holey_file {
    [int64]$size = (convert_to_bytes $args[0])
    # it causes CreateFile with CREATE_ALWAYS flag
    $mode = "-f"
    for ($i=1;$i -lt $args.count;$i++) {
        # need to call out to sparsefile.exe to create a sparse file, note
        # that initial version of DAX doesn't support sparse
        $fname = $args[$i]
        & $SPARSEFILE $mode $fname $size
        if ($Global:LASTEXITCODE -ne 0) {
            fatal "Error $Global:LASTEXITCODE with sparsefile create"
        }
        Get-ChildItem $fname >> $Env:PREP_LOG_FILE
    }
}

#
# create_nonzeroed_file -- create non-zeroed files of a given length
#
# A given first kilobytes of the file is zeroed out.
#
# example, to create two files, each 1GB in size, with first 4K zeroed
#	create_nonzeroed_file 1G 4K testfile1 testfile2
#
# Note: from 0 to offset is sparse, after that filled with Z
#
# Input unit size is in bytes with optional suffixes like k, KB, M, etc.
#
function create_nonzeroed_file {

    [int64]$offset = (convert_to_bytes $args[1])
    [int64]$size = ((convert_to_bytes $args[0]) - $offset)

    [int64]$numz =  $size / 1024
    [string] $z = "Z" * 1024 # using a 1K string to speed up writing
    for ($i=2;$i -lt $args.count;$i++) {
        # create sparse file of offset length
        $file = new-object System.IO.FileStream $args[$i], Create, ReadWrite
        $file.SetLength($offset)
        $file.Close()
        Get-ChildItem $args[$i] >> $Env:PREP_LOG_FILE
        $stream = new-object system.IO.StreamWriter($args[$i], "True", [System.Text.Encoding]::Ascii)
        1..$numz | %{ $stream.Write($Z) }
        $stream.close()
        Get-ChildItem $args[$i] >> $Env:PREP_LOG_FILE
    }
}

#
# create_poolset -- create a dummy pool set
#
# Creates a pool set file using the provided list of part sizes and paths.
# Optionally, it also creates the selected part files (zeroed, partially zeroed
# or non-zeroed) with requested size and mode.  The actual file size may be
# different than the part size in the pool set file.
# 'r' or 'R' on the list of arguments indicate the beginning of the next
# replica set and 'm' or 'M' the beginning of the next remote replica set.
# A remote replica requires two parameters: a target node and a pool set
# descriptor.
#
# Each part argument has the following format:
#   psize:ppath[:cmd[:fsize[:mode]]]
#
# where:
#   psize - part size or AUTO (only for DAX device)
#   ppath - path
#   cmd   - (optional) can be:
#            x - do nothing (may be skipped if there's no 'fsize', 'mode')
#            z - create zeroed (holey) file
#            n - create non-zeroed file
#            h - create non-zeroed file, but with zeroed header (first 4KB)
#            d - create empty directory
#   fsize - (optional) the actual size of the part file (if 'cmd' is not 'x')
#   mode  - (optional) same format as for 'chmod' command
#
# Each remote replica argument has the following format:
#   node:desc
#
# where:
#   node - target node
#   desc - pool set descriptor
#
# example:
#   The following command define a pool set consisting of two parts: 16MB
#   and 32MB, a local replica with only one part of 48MB and a remote replica.
#   The first part file is not created, the second is zeroed.  The only replica
#   part is non-zeroed. Also, the last file is read-only and its size
#   does not match the information from pool set file. The last line describes
#   a remote replica.
#
#	create_poolset .\pool.set 16M:testfile1 32M:testfile2:z \
#				R 48M:testfile3:n:11M:0400 \
#				M remote_node:remote_pool.set
#
#
function create_poolset {
    $psfile = $args[0]
    echo "PMEMPOOLSET" | out-file -encoding utf8 -literalpath $psfile
    for ($i=1;$i -lt $args.count;$i++) {
        if ($args[$i] -eq "M" -Or $args[$i] -eq 'm') { # remote replica
            $i++
            $cmd = $args[$i]
            $fparms = ($cmd.Split("{:}"))
            $node = $fparms[0]
            $desc = $fparms[1]
            echo "REPLICA $node $desc" | out-file -Append -encoding utf8 -literalpath $psfile
            continue
        }
        if ($args[$i] -eq "R" -Or $args[$i] -eq 'r') {
            echo "REPLICA" | out-file -Append -encoding utf8 -literalpath $psfile
            continue
        }
        if ($args[$i] -eq "O" -Or $args[$i] -eq 'o') {
            $i++
            $opt = $args[$i]
            echo "OPTION $opt" | out-file -Append -encoding utf8 -literalpath $psfile
            continue
        }

        $cmd = $args[$i]
        # need to strip out a drive letter if included because we use :
        # as a delimiter in the argument

        $driveLetter = ""
        if ($cmd -match ":([a-zA-Z]):\\") {
            # for path names in the following format: "C:\foo\bar"
            $tmp = ($cmd.Split("{:\\}",2,[System.StringSplitOptions]::RemoveEmptyEntries))
            $cmd = $tmp[0] + ":" + $tmp[1].SubString(2)
            $driveLetter = $tmp[1].SubString(0,2)
        } elseif ($cmd -match ":\\\\\?\\([a-zA-Z]):\\") {
            # for _long_ path names in the following format: "\\?\C:\foo\bar"
            $tmp = ($cmd.Split("{:}",2,[System.StringSplitOptions]::RemoveEmptyEntries))
            $cmd = $tmp[0] + ":" + $tmp[1].SubString(6)
            $driveLetter = $tmp[1].SubString(0,6)
        }
        $fparms = ($cmd.Split("{:}"))
        $fsize = $fparms[0]

        # XXX: unclear how to follow a symlink
        # like linux "fpath=`readlink -mn ${fparms[1]}`" but I've not tested
        # that it works with a symlink or shortcut
        $fpath = $fparms[1]
        if (-Not $driveLetter -eq "") {
            $fpath = $driveLetter + $fpath
        }
        $cmd = $fparms[2]
        $asize = $fparms[3]
        $mode = $fparms[4]

        if (-not $asize) {
            $asize = $fsize
        }

        switch -regex ($cmd) {
            # do nothing
            'x' { }
            # zeroed (holey) file
            'z' { create_holey_file $asize $fpath }
            # non-zeroed file
            'n' { create_file $asize $fpath }
            # non-zeroed file, except 4K header
            'h' { create_nonzeroed_file $asize 4K $fpath }
            # create empty directory
            'd' { new-item $fpath -force -itemtype directory >> $Env:PREP_LOG_FILE }
        }

        # XXX: didn't convert chmod
        # if [ $mode ]; then
        #     chmod $mode $fpath
        # fi

        echo "$fsize $fpath" | out-file -Append -encoding utf8 -literalpath $psfile
    } # for args
}

#
# dump_last_n_lines -- dumps the last N lines of given log file to stdout
#
function dump_last_n_lines {
    if ($Args[0] -And (Test-Path $Args[0])) {
        sv -Name fname ((Get-Location).path + "\" + $Args[0])
        sv -Name ln (getLineCount $fname)
        if ($ln -gt $UT_DUMP_LINES) {
            $ln = $UT_DUMP_LINES
            msg "Last $UT_DUMP_LINES lines of $fname below (whole file has $ln lines)."
        } else {
            msg "$fname below."
        }
        foreach ($line in Get-Content $fname -Tail $ln) {
            msg $line
        }
    }
}

#
# check_exit_code -- check if $LASTEXITCODE is equal 0
#
function check_exit_code {
    if ($Global:LASTEXITCODE -ne 0) {
        sv -Name msg "failed with exit code $Global:LASTEXITCODE"
        if (Test-Path $Env:ERR_LOG_FILE) {
            if ($Env:UNITTEST_LOG_LEVEL -ge "1") {
                echo "${Env:UNITTEST_NAME}: $msg. $Env:ERR_LOG_FILE" >> $Env:ERR_LOG_FILE
            } else {
                Write-Error "${Env:UNITTEST_NAME}: $msg.  $Env:ERR_LOG_FILE"
            }
        } else {
            Write-Error "${Env:UNITTEST_NAME}: $msg"
        }

        dump_last_n_lines $Env:PREP_LOG_FLE
        dump_last_n_lines $Env:TRACE_LOG_FILE
        dump_last_n_lines $Env:PMEM_LOG_FILE
        dump_last_n_lines $Env:PMEMOBJ_LOG_FILE
        dump_last_n_lines $Env:PMEMLOG_LOG_FILE
        dump_last_n_lines $Env:PMEMBLK_LOG_FILE
        dump_last_n_lines $Env:PMEMPOOL_LOG_FILE

        fail ""
    }
}

#
# expect_normal_exit -- run a given command, expect it to exit 0
#

function expect_normal_exit {
    #XXX:  bash sets up LD_PRELOAD and other gcc options here
    # that we can't do, investigating how to address API hooking...

    sv -Name command $args[0]
    $params = New-Object System.Collections.ArrayList
    foreach ($param in $Args[1 .. $Args.Count]) {
       if ($param -is [array]) {
            foreach ($param_entry in $param) {
                [string]$params += -join(" '", $param_entry, "' ")
            }
        } else {
            [string]$params += -join(" '", $param, "' ")
        }
    }

    # Set $LASTEXITCODE to the value indicating failure. It should be
    # overwritten with the exit status of the invoked command.
    # It is to catch the case when the command is not executed (i.e. because
    # of missing binaries / wrong path / etc.) and $LASTEXITCODE contains the
    # status of some other command executed before.
    $Global:LASTEXITCODE = 1
    Invoke-Expression "$command $params"

    check_exit_code
}

#
# expect_abnormal_exit -- run a given command, expect it to exit non-zero
#
function expect_abnormal_exit {
    #XXX:  bash sets up LD_PRELOAD and other gcc options here
    # that we can't do, investigating how to address API hooking...

    sv -Name command $args[0]
    $params = New-Object System.Collections.ArrayList
    foreach ($param in $Args[1 .. $Args.Count]) {
        if ($param -is [array]) {
            foreach ($param_entry in $param) {
                [string]$params += -join(" '", $param_entry, "' ")
            }
        } else {
            [string]$params += -join(" '", $param, "' ")
        }
    }

    # Suppress abort window
    $prev_abort = $Env:PMDK_NO_ABORT_MSG
    $Env:PMDK_NO_ABORT_MSG = 1

    # Set $LASTEXITCODE to the value indicating success. It should be
    # overwritten with the exit status of the invoked command.
    # It is to catch the case when the command is not executed (i.e. because
    # of missing binaries / wrong path / etc.) and $LASTEXITCODE contains the
    # status of some other command executed before.
    $Global:LASTEXITCODE = 0
    Invoke-Expression "$command $params"
    $Env:PMDK_NO_ABORT_MSG = $prev_abort
    if ($Global:LASTEXITCODE -eq 0) {
        fail "${Env:UNITTEST_NAME}: command succeeded unexpectedly."
    }
}

#
# check_pool -- run pmempool check on specified pool file
#
function check_pool {
    $file = $Args[0]
    if ($Env:CHECK_POOL -eq "1") {
        Write-Verbose "$Env:UNITTEST_NAME: checking consistency of pool $file"
        Invoke-Expression "$PMEMPOOL check $file 2>&1 1>>$Env:CHECK_POOL_LOG_FILE"
        if ($Global:LASTEXITCODE -ne 0) {
            fail "error: $PMEMPOOL returned error code ${Global:LASTEXITCODE}"
        }
    }
}

#
# check_pools -- run pmempool check on specified pool files
#
function check_pools {
    if ($Env:CHECK_POOL -eq "1") {
        foreach ($arg in $Args[0 .. $Args.Count]) {
            check_pool $arg
        }
    }
}

#
# require_unlimited_vm -- require unlimited virtual memory
#
# This implies requirements for:
# - overcommit_memory enabled (/proc/sys/vm/overcommit_memory is 0 or 1)
# - unlimited virtual memory (ulimit -v is unlimited)
#
function require_unlimited_vm {
    msg "${Env:UNITTEST_NAME}: SKIP required: overcommit_memory enabled and unlimited virtual memory"
    exit 0
}

#
# require_no_superuser -- require user without superuser rights
#
# XXX: not sure how to translate
#
function require_no_superuser {
    msg "${Env:UNITTEST_NAME}: SKIP required: run without superuser rights"
    exit 0
}

#
# require_test_type -- only allow script to continue for a certain test type
#
function require_test_type() {
    sv -Name req_test_type 1 -Scope Global

    if ($Env:TYPE -eq 'all') {
        return
    }

    for ($i=0;$i -lt $args.count;$i++) {
        if ($args[$i] -eq $Env:TYPE) {
            return
        }
        switch ($Env:TYPE) {
            'check' { # "check" is a synonym of "short + medium"
                if ($args[$i] -eq 'short' -Or $args[$i] -eq 'medium') {
                    return
                }
            }
            default {
                if ($args[$i] -eq $Env:TYPE) {
                    return
                }
            }
        }
        verbose_msg "${Env:UNITTEST_NAME}: SKIP test-type $Env:TYPE ($* required)"
        exit 0
    }
}

#
# require_build_type -- only allow script to continue for a certain build type
#
function require_build_type {
    for ($i=0;$i -lt $args.count;$i++) {
        if ($args[$i] -eq $Env:BUILD) {
            return
        }
    }

    verbose_msg "${Env:UNITTEST_NAME}: SKIP build-type $Env:BUILD ($* required)"
    exit 0
}

#
# require_pkg -- only allow script to continue if specified package exists
#
function require_pkg {
    # XXX: placeholder for checking dependencies if we have a need
}

#
# require_binary -- continue script execution only if the binary has been compiled
#
# In case of conditional compilation, skip this test.
#
function require_binary() {
    # XXX:  check if binary provided
    if (-Not (Test-Path $Args[0])) {
       msg "${Env:UNITTEST_NAME}: SKIP no binary found"
       exit 0
    }
}

#
# match -- execute match
#
function match {
    Invoke-Expression "perl ..\..\..\src\test\match $args"
    if ($Global:LASTEXITCODE -ne 0) {
        fail ""
    }
}

#
# check -- check test results (using .match files)
#
# note: win32 version slightly different since the caller can't as
# easily bail when a cmd fails
#
function check {
    #	..\match $(find . -regex "[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
    $perl = Get-Command -Name perl -ErrorAction SilentlyContinue
    If ($perl -eq $null) {
        fail "error: Perl is missing, cannot check test results"
    }

    # If errX.log.match does not exist, assume errX.log should be empty
    $ERR_LOG_LEN=0
    if (Test-Path $Env:ERR_LOG_FILE) {
        $ERR_LOG_LEN = (Get-Item  $Env:ERR_LOG_FILE).length
    }

    if (-not (Test-Path "${Env:ERR_LOG_FILE}.match") -and ($ERR_LOG_LEN -ne 0)) {
        Write-Error "unexpected output in ${Env:ERR_LOG_FILE}"
        dump_last_n_lines $Env:ERR_LOG_FILE
        fail ""
    }

    [string]$listing = Get-ChildItem -File | Where-Object {$_.Name -match "[^0-9]${Env:UNITTEST_NUM}.log.match"}
    if ($listing) {
		match $listing
    }
}

#
# pass -- print message that the test has passed
#
function pass {
    if ($Env:TM -eq 1) {
        $end_time = $script:tm.Elapsed.ToString('ddd\:hh\:mm\:ss\.fff') -Replace "^(000:)","" -Replace "^(00:){1,2}",""
        $script:tm.reset()
    } else {
        sv -Name end_time $null
    }

    if ($Env:UNITTEST_LOG_LEVEL -ge "1") {
        Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
        Write-Host -NoNewline -foregroundcolor green "PASS"
        if ($end_time) {
            Write-Host -NoNewline ("`t`t`t" + "[" + $end_time + " s]")
        }
    }

    if ($Env:FS -ne "none") {
        if (isDir $DIR) {
             rm -Force -Recurse $DIR
        }
    }

    msg ""
}

#
# fail -- print message that the test has failed
#
function fail {
    Write-Error $args[0]
    Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
    Write-Host -NoNewLine -foregroundcolor red "FAILED"
    throw "${Env:UNITTEST_NAME}: FAILED"
}

#
# remove_files - removes list of files included in variable
#
function remove_files {
    for ($i=0;$i -lt $args.count;$i++) {
        $arr = $args[$i] -split ' '
        ForEach ($file In $arr) {
            Remove-Item $file -Force -ea si
        }
    }
}

#
# check_file -- check if file exists and print error message if not
#
function check_file {
    sv -Name fname $Args[0]
    if (-Not (Test-Path $fname)) {
        fail "error: Missing File: $fname"
    }
}

#
# check_files -- check if files exist and print error message if not
#
function check_files {
    for ($i=0;$i -lt $args.count;$i++) {
        check_file $args[$i]
    }
}

#
# check_no_file -- check if file has been deleted and print error message if not
#
function check_no_file {
    sv -Name fname $Args[0]
    if (Test-Path $fname) {
        fail "error: Not deleted file: $fname"
    }
}

#
# check_no_files -- check if files has been deleted and print error message if not
#
function check_no_files {
    for ($i=0;$i -lt $args.count;$i++) {
        check_no_file $args[$i]
    }
}

#
# get_size -- return size of file
#
function get_size {
    if (Test-Path $args[0]) {
        return (Get-Item $args[0]).length
    }
}

#
# set_file_mode - set access mode to one or multiple files
# parameters:
# arg0 - access mode you want to change
# arg1 - true or false to admit or deny given mode
#
# example:
# set_file_mode IsReadOnly $true file1 file2
#
function set_file_mode {
    $mode = $args[0]
    $flag = $args[1]
    for ($i=2;$i -lt $args.count;$i++) {
        Set-ItemProperty $args[$i] -name $mode -value $flag
    }
}

#
# get_mode -- return mode of file
#
function get_mode {
    if (Test-Path $args[0]) {
        return (Get-Item $args[0]).mode
    }
}

#
# check_size -- validate file size
#
function check_size {
    sv -Name size -Scope "Local" $args[0]
    sv -Name file -Scope "Local" $args[1]
    sv -Name file_size -Scope "Local" (get_size $file)

    if ($file_size -ne $size) {
        fail "error: wrong size $file_size != $size"
    }
}

#
# check_mode -- validate file mode
#
function check_mode {
    sv -Name mode -Scope "Local" $args[0]
    sv -Name file -Scope "Local" $args[1]
    $mode = [math]::floor($mode / 100) # get first digit (user/owner permission)
    $read_only = (gp $file IsReadOnly).IsReadOnly

    if ($mode -band 2) {
        if ($read_only -eq $true) {
            fail "error: wrong file mode"
        } else {
            return
        }
    }
    if ($read_only -eq $false) {
        fail "error: wrong file mode"
    } else {
        return
    }
}

#
# check_signature -- check if file contains specified signature
#
function check_signature {
    sv -Name sig -Scope "Local" $args[0]
    sv -Name file -Scope "Local" ($args[1])
    sv -Name file_sig -Scope "Local" ""
    $stream = [System.IO.File]::OpenRead($file)
    $buff = New-Object Byte[] $SIG_LEN
    # you must assign return value otherwise PS will print it to stdout
    $num = $stream.Read($buff, 0, $SIG_LEN)
    $file_sig = [System.Text.Encoding]::Ascii.GetString($buff)
    $stream.Close()
    if ($file_sig -ne $sig) {
        fail "error: $file signature doesn't match $file_sig != $sig"
    }
}

#
# check_signatures -- check if multiple files contain specified signature
#
function check_signatures {
    for ($i=1;$i -lt $args.count;$i+=1) {
        check_signature $args[0] $args[$i]
    }
}

#
# check_layout -- check if pmemobj pool contains specified layout
#
function check_layout {
    sv -Name layout -Scope "Local" $args[0]
    sv -Name file -Scope "Local" ($args[1])

    $stream = [System.IO.File]::OpenRead($file)
    $stream.Position = $LAYOUT_OFFSET
    $buff = New-Object Byte[] $LAYOUT_LEN
    # you must assign return value otherwise PS will print it to stdout
    $num = $stream.Read($buff, 0, $LAYOUT_LEN)
    $enc = [System.Text.Encoding]::UTF8.GetString($buff)
    $stream.Close()
    if ($enc -ne $layout) {
        fail "error: layout doesn't match $enc != $layout"
    }
}

#
# check_arena -- check if file contains specified arena signature
#
function check_arena {
    sv -Name file -Scope "Local" ($args[0])

    $stream = [System.IO.File]::OpenRead($file)
    $stream.Position = $ARENA_OFF
    $buff = New-Object Byte[] $ARENA_SIG_LEN
    # you must assign return value otherwise PS will print it to stdout
    $num = $stream.Read($buff, 0, $ARENA_SIG_LEN)
    $enc = [System.Text.Encoding]::ASCII.GetString($buff)
    $stream.Close()
    if ($enc -ne $ARENA_SIG) {
        fail "error: can't find arena signature"
    }
}

#
# dump_pool_info -- dump selected pool metadata and/or user data
#
function dump_pool_info {
    $params = ""
    for ($i=0;$i -lt $args.count;$i++) {
        [string]$params += -join($args[$i], " ")
    }

    # ignore selected header fields that differ by definition
    # this is equivalent of: 'sed -e "/^UUID/,/^Checksum/d"'
    $print = $True
    Invoke-Expression "$PMEMPOOL info $params" | % {
        If ($_ -match '^UUID') {
            $print = $False
        }
        If ($print -eq $True) {
            $_
        }
        If ($_ -match '^Checksum') {
            $print = $True
        }
    }
}

#
# dump_replica_info -- dump selected pool metadata and/or user data
#
# Used by compare_replicas() - filters out file paths and sizes.
#
function dump_replica_info {
    $params = ""
    for ($i=0;$i -lt $args.count;$i++) {
        [string]$params += -join($args[$i], " ")
    }

    # ignore selected header fields that differ by definition
    # this is equivalent of: 'sed -e "/^UUID/,/^Checksum/d"'
    $print = $True
    Invoke-Expression "$PMEMPOOL info $params" | % {
        If ($_ -match '^UUID') {
            $print = $False
        }
        If ($print -eq $True) {
            # 'sed -e "/^path/d" -e "/^size/d"
            If (-not ($_ -match '^path' -or  $_ -match '^size')) {
                $_
            }
        }
        If ($_ -match '^Checksum') {
            $print = $True
        }
    }
}

#
# compare_replicas -- check replicas consistency by comparing `pmempool info` output
#
function compare_replicas {
    $count = $args

    foreach ($param in $Args[0 .. ($Args.Count - 3)]) {
        if ($param -is [array]) {
            foreach ($param_entry in $param) {
                [string]$params += -join(" '", $param_entry, "' ")
            }
        } else {
            [string]$params += -join($param, " ")
        }
    }

    $rep1 = $args[$cnt + 1]
    $rep2 = $args[$cnt + 2]

    diff (dump_replica_info $params $rep1) (dump_replica_info $params $rep2)
}

#
# require_fs_type -- only allow script to continue for a certain fs type
#
function require_fs_type {
    $Global:req_fs_type = 1

    for ($i = 0; $i -lt $args.count; $i++) {
        $type = $args[$i]

        # treat 'any' as either 'pmem' or 'non-pmem'
        if (($type -eq $Env:FS) -or (($type -eq "any") -and ($Env:FS -ne "none") -and $Env:FORCE_FS -eq 1)) {
		return;
        }
    }
    verbose_msg "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
    exit 0
}

#
# require_dax_devices -- only allow script to continue for a dax device
#
function require_dax_devices() {
    # XXX: no device dax on Windows
    msg "${Env:UNITTEST_NAME}: SKIP DEVICE_DAX_PATH does not specify enough dax devices"
    exit 0
}

function dax_device_zero() {
    # XXX: no device dax on Windows
}

#
# require_no_unicode -- require $DIR w/o non-ASCII characters
#
function require_no_unicode {
    $Env:SUFFIX = ""

    $u = [System.Text.Encoding]::UNICODE

    [string]$DIR_ASCII = [System.Text.Encoding]::Convert([System.Text.Encoding]::UNICODE,
            [System.Text.Encoding]::ASCII, $u.getbytes($DIR))
    [string]$DIR_UTF8 = [System.Text.Encoding]::Convert([System.Text.Encoding]::UNICODE,
            [System.Text.Encoding]::UTF8, $u.getbytes($DIR))

    if ($DIR_UTF8 -ne $DIR_ASCII) {
        msg "${Env:UNITTEST_NAME}: SKIP required: test directory path without non-ASCII characters"
        exit 0
    }
}

#
# require_short_path -- require $DIR length less than 256 characters
#
function require_short_path {
    $Env:DIRSUFFIX = ""

    if ($DIR.Length -ge 256) {
        msg "${Env:UNITTEST_NAME}: SKIP required: test directory path below 256 characters"
        exit 0
    }
}

#
# get_files -- returns all files in cwd with given pattern
#
function get_files {
    dir |% {$_.Name} | select-string -Pattern $args[0]
}

#
# setup -- print message that test setup is commencing
#
function setup {

    $curtestdir = (Get-Item -Path ".\").BaseName

    # just in case
    if (-Not $curtestdir) {
        fatal "curtestdir does not exist"
    }

    $curtestdir = "test_" + $curtestdir

    $Script:DIR = $DIR + "\" + $Env:DIRSUFFIX + "\" + $curtestdir + $Env:UNITTEST_NUM + $Env:SUFFIX

    # test type must be explicitly specified
    if ($req_test_type -ne "1") {
        fatal "error: required test type is not specified"
    }

    # fs type "none" must be explicitly enabled
    if ($Env:FS -eq "none" -and $Global:req_fs_type -ne "1") {
        exit 0
    }

    # fs type "any" must be explicitly enabled
    if ($Env:FS -eq "any" -and $Global:req_fs_type -ne "1") {
        exit 0
    }

    msg "${Env:UNITTEST_NAME}: SETUP ($Env:TYPE\$Global:REAL_FS\$Env:BUILD)"

    foreach ($f in $(get_files "[a-zA-Z_]*${Env:UNITTEST_NUM}\.log$")) {
        Remove-Item $f
    }

    rm -Force check_pool_${Env:BUILD}_${Env:UNITTEST_NUM}.log -ErrorAction SilentlyContinue

    if ($Env:FS -ne "none") {

        if (isDir $DIR) {
             rm -Force -Recurse $DIR
        }
        md -force $DIR > $null
    }

    # XXX: do it before setup() is invoked
    # set console encoding to UTF-8
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8

    if ($Env:TM -eq "1" ) {
        $script:tm = [system.diagnostics.stopwatch]::startNew()
    }

    $DEBUG_DIR = '..\..\x64\Debug'
    $RELEASE_DIR = '..\..\x64\Release'

    if ($Env:BUILD -eq 'nondebug') {
        if (-Not $Env:PMDK_LIB_PATH_NONDEBUG) {
            $Env:PMDK_LIB_PATH_NONDEBUG = $RELEASE_DIR + '\libs\'
        }
        $Env:Path = $Env:PMDK_LIB_PATH_NONDEBUG + ';' + $Env:Path
    } elseif ($Env:BUILD -eq 'debug') {
        if (-Not $Env:PMDK_LIB_PATH_DEBUG) {
            $Env:PMDK_LIB_PATH_DEBUG = $DEBUG_DIR + '\libs\'
        }
        $Env:Path = $Env:PMDK_LIB_PATH_DEBUG + ';' + $Env:Path
    }

	$Env:PMEMBLK_CONF="fallocate.at_create=0;"
	$Env:PMEMOBJ_CONF="fallocate.at_create=0;"
	$Env:PMEMLOG_CONF="fallocate.at_create=0;"
}

#
# cmp -- compare two files
#
function cmp {
    $file1 = $Args[0]
    $file2 = $Args[1]
    $argc = $Args.Count

    if($argc -le 2) {
        # fc does not support / in file path
        fc.exe /b ([String]$file1).Replace('/','\') ([string]$file2).Replace('/','\') > $null
        if ($Global:LASTEXITCODE -ne 0) {
            "$args differ"
        }
        return
    }
    $limit = $Args[2]
    $s1 = Get-Content $file1 -totalcount $limit -encoding byte
    $s2 = Get-Content $file1 -totalcount $limit -encoding byte
    if ("$s1" -ne "$s2") {
        "$args differ"
    }

}
#######################################################
#######################################################

if (-Not $Env:UNITTEST_NAME) {
    $CURDIR = (Get-Item -Path ".\").BaseName
    $SCRIPTNAME = (Get-Item $MyInvocation.ScriptName).BaseName

    $Env:UNITTEST_NAME = "$CURDIR/$SCRIPTNAME"
    $Env:UNITTEST_NUM = ($SCRIPTNAME).Replace("TEST", "")
}

# defaults
if (-Not $Env:TYPE) { $Env:TYPE = 'check'}
if (-Not $Env:FS) { $Env:FS = 'any'}
if (-Not $Env:BUILD) { $Env:BUILD = 'debug'}
if (-Not $Env:CHECK_POOL) { $Env:CHECK_POOL = '0'}
if (-Not $Env:EXESUFFIX) { $Env:EXESUFFIX = ".exe"}
if (-Not $Env:SUFFIX) { $Env:SUFFIX = "😘⠏⠍⠙⠅ɗPMDKӜ⥺🙋"}
if (-Not $Env:DIRSUFFIX) { $Env:DIRSUFFIX = ""}

if ($Env:BUILD -eq 'nondebug') {
    if (-Not $Env:PMDK_LIB_PATH_NONDEBUG) {
        $PMEMPOOL = $RELEASE_DIR + "\libs\pmempool$Env:EXESUFFIX"
    } else {
        $PMEMPOOL = "$Env:PMDK_LIB_PATH_NONDEBUG\pmempool$Env:EXESUFFIX"
    }
} elseif ($Env:BUILD -eq 'debug') {
    if (-Not $Env:PMDK_LIB_PATH_DEBUG) {
        $PMEMPOOL = $DEBUG_DIR + "\libs\pmempool$Env:EXESUFFIX"
    } else {
        $PMEMPOOL = "$Env:PMDK_LIB_PATH_DEBUG\pmempool$Env:EXESUFFIX"
    }
}

$PMEMSPOIL="$Env:EXE_DIR\pmemspoil$Env:EXESUFFIX"
$PMEMWRITE="$Env:EXE_DIR\pmemwrite$Env:EXESUFFIX"
$PMEMALLOC="$Env:EXE_DIR\pmemalloc$Env:EXESUFFIX"
$PMEMOBJCLI="$Env:EXE_DIR\pmemobjcli$Env:EXESUFFIX"
$DDMAP="$Env:EXE_DIR\ddmap$Env:EXESUFFIX"
$BTTCREATE="$Env:EXE_DIR\bttcreate$Env:EXESUFFIX"

$SPARSEFILE="$Env:EXE_DIR\sparsefile$Env:EXESUFFIX"
$DLLVIEW="$Env:EXE_DIR\dllview$Env:EXESUFFIX"

$Global:req_fs_type=0

#
# The variable DIR is constructed so the test uses that directory when
# constructing test files.  DIR is chosen based on the fs-type for
# this test, and if the appropriate fs-type doesn't have a directory
# defined in testconfig.sh, the test is skipped.

if (-Not $Env:UNITTEST_NUM) {
    fatal "UNITTEST_NUM does not have a value"
}

if (-Not $Env:UNITTEST_NAME) {
    fatal "UNITTEST_NAME does not have a value"
}

$Global:REAL_FS = $Env:FS

# choose based on FS env variable
switch ($Env:FS) {
    'pmem' {
        # if a variable is set - it must point to a valid directory
        if (-Not $Env:PMEM_FS_DIR) {
            fatal "${Env:UNITTEST_NAME}: PMEM_FS_DIR not set"
        }
        sv -Name DIR $Env:PMEM_FS_DIR
        if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
            $Env:PMEM_IS_PMEM_FORCE = "1"
        }
    }
    'non-pmem' {
        # if a variable is set - it must point to a valid directory
        if (-Not $Env:NON_PMEM_FS_DIR) {
            fatal "${Env:UNITTEST_NAME}: NON_PMEM_FS_DIR not set"
        }
        sv -Name DIR $Env:NON_PMEM_FS_DIR
    }
    'any' {
         if ($Env:PMEM_FS_DIR) {
            sv -Name DIR ($Env:PMEM_FS_DIR + $tail)
            $Global:REAL_FS='pmem'
            if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
                $Env:PMEM_IS_PMEM_FORCE = "1"
            }
        } ElseIf ($Env:NON_PMEM_FS_DIR) {
            sv -Name DIR $Env:NON_PMEM_FS_DIR
            $Global:REAL_FS='non-pmem'
        } Else {
            fatal "${Env:UNITTEST_NAME}: fs-type=any and both env vars are empty"
        }
    }
    'none' {
        # don't add long path nor unicode sufix to DIR
        require_no_unicode
        require_short_path
        sv -Name DIR "\nul\not_existing_dir\"
    }
    default {
        fatal "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
    }
} # switch

# Length of pool file's signature
sv -Name SIG_LEN 8

# Offset and length of pmemobj layout
sv -Name LAYOUT_OFFSET 4096
sv -Name LAYOUT_LEN 1024

# Length of arena's signature
sv -Name ARENA_SIG_LEN 16

# Signature of BTT Arena
sv -Name ARENA_SIG "BTT_ARENA_INFO"

# Offset to first arena
sv -Name ARENA_OFF 8192

#
# The default is to turn on library logging to level 3 and save it to local files.
# Tests that don't want it on, should override these environment variables.
#
$Env:PMEM_LOG_LEVEL = 3
$Env:PMEM_LOG_FILE = "pmem${Env:UNITTEST_NUM}.log"
$Env:PMEMBLK_LOG_LEVEL=3
$Env:PMEMBLK_LOG_FILE = "pmemblk${Env:UNITTEST_NUM}.log"
$Env:PMEMLOG_LOG_LEVEL = 3
$Env:PMEMLOG_LOG_FILE = "pmemlog${Env:UNITTEST_NUM}.log"
$Env:PMEMOBJ_LOG_LEVEL = 3
$Env:PMEMOBJ_LOG_FILE= "pmemobj${Env:UNITTEST_NUM}.log"
$Env:PMEMPOOL_LOG_LEVEL = 3
$Env:PMEMPOOL_LOG_FILE= "pmempool${Env:UNITTEST_NUM}.log"

$Env:TRACE_LOG_FILE = "trace${Env:UNITTEST_NUM}.log"
$Env:ERR_LOG_FILE = "err${Env:UNITTEST_NUM}.log"
$Env:OUT_LOG_FILE = "out${Env:UNITTEST_NUM}.log"
$Env:PREP_LOG_FILE = "prep${Env:UNITTEST_NUM}.log"

if (-Not($UT_DUMP_LINES)) {
    sv -Name "UT_DUMP_LINES" 30
}

$Env:CHECK_POOL_LOG_FILE = "check_pool_${Env:BUILD}_${Env:UNITTEST_NUM}.log"

#
# enable_log_append -- turn on appending to the log files rather than truncating them
# It also removes all log files created by tests: out*.log, err*.log and trace*.log
#
function enable_log_append() {
    rm -Force -ErrorAction SilentlyContinue $Env:OUT_LOG_FILE
    rm -Force -ErrorAction SilentlyContinue $Env:ERR_LOG_FILE
    rm -Force -ErrorAction SilentlyContinue $Env:TRACE_LOG_FILE
    rm -Force -ErrorAction SilentlyContinue $Env:PREP_LOG_FILE
    $Env:UNITTEST_LOG_APPEND=1
}

#
# require_free_space -- check if there is enough free space to run the test
# Example, checking if there is 1 GB of free space on disk:
# require_free_space 1G
#
function require_free_space() {
	$req_free_space = (convert_to_bytes $args[0])

	# actually require 5% or 8MB (whichever is higher) more, just in case
	# file system requires some space for its meta data
	$pct = 5 * $req_free_space / 100
	$abs = (convert_to_bytes 8M)
	if ($pct -gt $abs) {
		$req_free_space = $req_free_space + $pct
	} else {
		$req_free_space = $req_free_space + $abs
	}

	$path = $DIR -replace '\\\\\?\\', ''
	$device_name = (Get-Item $path).PSDrive.Root
	$filter = "Name='$($device_name -replace '\\', '\\')'"
	$free_space = (gwmi Win32_Volume -Filter $filter | select FreeSpace).freespace
	if ([INT64]$free_space -lt [INT64]$req_free_space)
	{
		msg "${Env:UNITTEST_NAME}: SKIP not enough free space ($args required)"
		exit 0
	}
}

#
# require_free_physical_memory -- check if there is enough free physical memory
# space to run the test
# Example, checking if there is 1 GB of free physical memory space:
# require_free_physical_memory 1G
#
function require_free_physical_memory() {
	$req_free_physical_memory = (convert_to_bytes $args[0])
	$free_physical_memory = (Get-CimInstance Win32_OperatingSystem | Select-Object -ExpandProperty FreePhysicalMemory) * 1024

	if ($free_physical_memory -lt $req_free_physical_memory)
	{
		msg "${Env:UNITTEST_NAME}: SKIP not enough free physical memory ($args required, free: $free_physical_memory B)"
		exit 0
	}
}

#
# require_automatic_managed_pagefile -- check if system manages the page file size
#
function require_automatic_managed_pagefile() {
	$c = Get-WmiObject Win32_computersystem -EnableAllPrivileges
	if($c.AutomaticManagedPagefile -eq $false) {
		msg "${Env:UNITTEST_NAME}: SKIP automatic page file management is disabled"
		exit 0
	}
}
