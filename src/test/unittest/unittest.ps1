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

. "..\testconfig.ps1"

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
    if ((Get-Item $args[0] -ErrorAction SilentlyContinue) -is [System.IO.DirectoryInfo]) {
        return $true
    } Else {
        return $false
    }
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
        Write-Error "Error suspicious byte value to convert_to_bytes"
        exit 1
    }

    return $size
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
        & $SPARSEFILE $fname $size_in_bytes
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
        Get-ChildItem $args[$i]* >> ("prep" + $Env:UNITTEST_NUM + ".log")
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
            throw "Error $Global:LASTEXITCODE with sparsefile create"
        }
        Get-ChildItem $fname >> ("prep" + $Env:UNITTEST_NUM + ".log")
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
    [string] $z = "Z" * 1024 # using a 1K string to speed up writting
    for ($i=2;$i -lt $args.count;$i++) {
        # create sparse file of offset length
        $file = new-object System.IO.FileStream $args[$i], Create, ReadWrite
        $file.SetLength($offset)
        $file.Close()
        Get-ChildItem $args[$i] >> ("prep" + $Env:UNITTEST_NUM + ".log")
        $stream = new-object system.IO.StreamWriter($args[$i], "True", [System.Text.Encoding]::Ascii)
        1..$numz | %{ $stream.Write($Z) }
        $stream.close()
        Get-ChildItem $args[$i] >> ("prep" + $Env:UNITTEST_NUM + ".log")
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
        $cmd = $args[$i]
        # need to strip out a drive letter if included because we use :
        # as a delimeter in the argument

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
    if (Test-Path $Args[0]) {
        sv -Name fname ((Get-Location).path + "\" + $Args[0])
        sv -Name ln (getLineCount $fname)
        if ($ln -gt $UT_DUMP_LINES) {
            $ln = $UT_DUMP_LINES
            Write-Host "Last $UT_DUMP_LINES lines of $fname below (whole file has $ln lines)."
        } else {
            Write-Host "$fname below."
        }
        foreach ($line in Get-Content $fname -Tail $ln) {
            Write-Host $line
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
            if ($Env:UNITTEST_QUIET) {
                echo "${Env:UNITTEST_NAME}: $msg. $Env:ERR_LOG_FILE" >> $Env:ERR_LOG_FILE
            } else {
                Write-Error "${Env:UNITTEST_NAME}: $msg.  $Env:ERR_LOG_FILE"
            }
        } else {
            Write-Error "${Env:UNITTEST_NAME}: $msg"
        }

        # XXX: if we implement a memcheck thing...
        # if [ "$RUN_MEMCHECK" ]; then

        dump_last_n_lines $Env:TRACE_LOG_FILE
        dump_last_n_lines $Env:PMEM_LOG_FILE
        dump_last_n_lines $Env:PMEMOBJ_LOG_FILE
        dump_last_n_lines $Env:PMEMLOG_LOG_FILE
        dump_last_n_lines $Env:PMEMBLK_LOG_FILE
        dump_last_n_lines $Env:PMEMPOOL_LOG_FILE
        dump_last_n_lines $Env:VMEM_LOG_FILE
        dump_last_n_lines $Env:VMMALLOC_LOG_FILE

        fail 1
    }
}

#
# expect_normal_exit -- run a given command, expect it to exit 0
#

function expect_normal_exit {
    #XXX: add memcheck eq checks for windows once we get one
    # if [ "$RUN_MEMCHECK" ]; then...

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
    # XXX: if we implement a memcheck thing... set some env vars here
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

    # Set $LASTEXITCODE to the value indicating success. It should be
    # overwritten with the exit status of the invoked command.
    # It is to catch the case when the command is not executed (i.e. because
    # of missing binaries / wrong path / etc.) and $LASTEXITCODE contains the
    # status of some other command executed before.
    $Global:LASTEXITCODE = 0
    Invoke-Expression "$command $params"
    if ($Global:LASTEXITCODE -eq 0) {
        Write-Error "${Env:UNITTEST_NAME}: command succeeded unexpectedly."
        fail 1
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
            Write-Error("$PMEMPOOL returned error code $Global:LASTEXITCODE")
            Exit $Global:LASTEXITCODE
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
    Write-Host "${Env:UNITTEST_NAME}: SKIP required: overcommit_memory enabled and unlimited virtual memory"
    exit 0
}

#
# require_no_superuser -- require user without superuser rights
#
# XXX: not sure how to translate
#
function require_no_superuser {
    Write-Host "${Env:UNITTEST_NAME}: SKIP required: run without superuser rights"
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
        if (-Not $Env:UNITTEST_QUIET) {
            echo "${Env:UNITTEST_NAME}: SKIP test-type $Env:TYPE ($* required)"
        }
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

    if (-Not $Env:UNITTEST_QUIET) {
        echo "${Env:UNITTEST_NAME}: SKIP build-type $Env:BUILD ($* required)"
    }
    exit 0
}

#
# require_pkg -- only allow script to continue if specified package exists
#
function require_pkg {
    # XXX: placeholder for checking dependencies if we have a need
}

#
# memcheck -- only allow script to continue when memcheck's settings match
#
function memcheck {
    # XXX: placeholder
}

#
# require_binary -- continue script execution only if the binary has been compiled
#
# In case of conditional compilation, skip this test.
#
function require_binary() {
    if (-Not (Test-Path $Args[0])) {
       if (-Not $Env:UNITTEST_QUIET) {
            Write-Host "${Env:UNITTEST_NAME}: SKIP no binary found"
       }
       exit 0
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
        Write-Error "Perl is missing, cannot check test results"
        fail 1
    }
    [string]$listing = Get-ChildItem -File | Where-Object  {$_.Name -match "[^0-9]${Env:UNITTEST_NUM}.log.match"}
    if ($listing) {
        Invoke-Expression "perl ..\..\..\src\test\match $listing"
        if ($Global:LASTEXITCODE -ne 0) {
            fail 1
        }

    } else {
        Write-Error "No match file found for test $Env:UNITTEST_NAME"
        fail 1
    }
}

#
# pass -- print message that the test has passed
#
function pass {
    if ($Env:TM -eq 1) {
        $end_time = $script:tm.Elapsed.ToString('hh\:mm\:ss\.fff') -Replace "^(00:){1,2}",""
        $script:tm.reset()
    } else {
        sv -Name end_time $null
    }

    sv -Name msg "PASS"
    Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
    Write-Host -NoNewline -foregroundcolor green $msg
    if ($end_time) {
        Write-Host -NoNewline ("`t`t`t" + "[" + $end_time + " s]")
    }

    if ($Env:FS -ne "none") {
        if (isDir $DIR) {
             rm -Force -Recurse $DIR
        }
    }
    Write-Host ""
}

#
# fail -- print message that the test has failed
#
function fail {
    sv -Name msg "FAILED"
    Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
    Write-Host -NoNewLine -foregroundcolor red $msg
    Write-Host (" with errorcode " + $args[0])
    throw $Env:UNITTEST_NAME + ": FAILED with errorcode $args[0]"
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
        Write-Error "Missing File: $fname"
        fail 1
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
        Write-Error "Not deleted file: $fname"
        fail 1
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
        Write-Error "error: wrong size $file_size != $size"
        fail 1
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
            Write-Error "error: wrong file mode"
            fail 1
        } else {
            return
        }
    }
    if ($read_only -eq $false) {
        Write-Error "error: wrong file mode"
        fail 1
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
        Write-Error "error: $file signature doesn't match $file_sig != $sig"
        fail 1
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
        Write-Error "error: layout doesn't match $enc != $layout"
        fail 1
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
        Write-Error "error: can't find arena signature"
        fail 1
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
# require_pmem -- only allow script to continue for a real PMEM device
#
function require_pmem {
    # note: PMEM_IS_PMEM 0 means it is PMEM, 1 means it is not
    if ($Global:PMEM_IS_PMEM -eq "0") {
        return $true
    } else {
        throw "error: PMEM_FS_DIR=$Env:PMEM_FS_DIR does not point to a PMEM device"
    }
}

#
# require_non_pmem -- only allow script to continue for a non-PMEM device
#
function require_non_pmem {
    if ($Global:NON_PMEM_IS_PMEM -eq "1") {
        return $true
    } else {
        throw "error: NON_PMEM_FS_DIR=$Env:NON_PMEM_FS_DIR does not point to a non-PMEM device"
    }
}

#
# require_fs_type -- only allow script to continue for a certain fs type
#
function require_fs_type {
    $Global:req_fs_type = 1

    for ($i = 0; $i -lt $args.count; $i++) {
        $type = $args[$i]

        # treat 'any' as either 'pmem' or 'non-pmem'
        if (($type -eq $Env:FS) -or (($type -eq "any") -and ($Env:FS -ne "none"))) {
            switch ($Global:REAL_FS) {
                'pmem' { if (require_pmem) { return } }
                'non-pmem' { if (require_non_pmem) { return } }
                'none' { return }
            }
        }
    }
    if (-Not $Env:UNITTEST_QUIET) {
        Write-Host "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
    }
    exit 0
}

#
# require_dax_devices -- only allow script to continue for a dax device
#
function require_dax_devices() {
    # XXX: no device dax on Windows
    if (-Not $Env:UNITTEST_QUIET) {
        Write-Host "${Env:UNITTEST_NAME}: SKIP DEVICE_DAX_PATH does not specify enough dax devices"
    }
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
        #if (-Not $Env:UNITTEST_QUIET) {
            Write-Host "${Env:UNITTEST_NAME}: SKIP required: test directory path without non-ASCII characters"
        #}
        exit 0
    }
}

#
# require_short_path -- require $DIR length less than 256 characters
#
function require_short_path {
    $Env:DIRSUFFIX = ""

    if ($DIR.Length -ge 256) {
        if (-Not $Env:UNITTEST_QUIET) {
            Write-Host "${Env:UNITTEST_NAME}: SKIP required: test directory path below 256 characters"
        }
        exit 0
    }
}

#
# setup -- print message that test setup is commencing
#
function setup {
    $Script:DIR = $DIR + $Env:SUFFIX

    # test type must be explicitly specified
    if ($req_test_type -ne "1") {
        throw "error: required test type is not specified"
    }

    # fs type "none" must be explicitly enabled
    if ($Env:FS -eq "none" -and $Global:req_fs_type -ne "1") {
        exit 0
    }

    # fs type "any" must be explicitly enabled
    if ($Env:FS -eq "any" -and $Global:req_fs_type -ne "1") {
        exit 0
    }

    # XXX: don't think we have a memcheck eq for windows yet but
    # will leave the logic in here in case we find something to use
    # that we can flip on/off with a flag
    if ($MEMCHECK -eq "force-enable") { $Env:RUN_MEMCHECK = 1 }

    if ($RUN_MEMCHECK) {
        sv -Name MCSTR "/memcheck"
    } else {
        sv -Name MCSTR ""
    }

    Write-Host "${Env:UNITTEST_NAME}: SETUP ($Env:TYPE\$Global:REAL_FS\$Env:BUILD$MCSTR)"

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

# defaults
if (-Not $Env:TYPE) { $Env:TYPE = 'check'}
if (-Not $Env:FS) { $Env:FS = 'any'}
if (-Not $Env:BUILD) { $Env:BUILD = 'debug'}
if (-Not $Env:MEMCHECK) { $Env:MEMCHECK = 'auto'}
if (-Not $Env:CHECK_POOL) { $Env:CHECK_POOL = '0'}
if (-Not $Env:EXESUFFIX) { $Env:EXESUFFIX = ".exe"}
if (-Not $Env:SUFFIX) { $Env:SUFFIX = "😘⠝⠧⠍⠇ɗNVMLӜ⥺🙋"}
if (-Not $Env:DIRSUFFIX) { $Env:DIRSUFFIX = ""}

if ($Env:EXE_DIR -eq $null) {
    $Env:EXE_DIR = "..\..\x64\debug"
}

$PMEMPOOL="$Env:EXE_DIR\pmempool$Env:EXESUFFIX"
$PMEMSPOIL="$Env:EXE_DIR\pmemspoil$Env:EXESUFFIX"
$PMEMWRITE="$Env:EXE_DIR\pmemwrite$Env:EXESUFFIX"
$PMEMALLOC="$Env:EXE_DIR\pmemalloc$Env:EXESUFFIX"
$PMEMDETECT="$Env:EXE_DIR\pmemdetect$Env:EXESUFFIX"
$PMEMOBJCLI="$Env:EXE_DIR\pmemobjcli$Env:EXESUFFIX"
$DDMAP="$Env:EXE_DIR\ddmap$Env:EXESUFFIX"
$BTTCREATE="$Env:EXE_DIR\bttcreate$Env:EXESUFFIX"

$SPARSEFILE="$Env:EXE_DIR\sparsefile$Env:EXESUFFIX"
$DLLVIEW="$Env:EXE_DIR\dllview$Env:EXESUFFIX"

$Global:req_fs_type=0

#
# For non-static build testing, the variable TEST_LD_LIBRARY_PATH is
# constructed so the test pulls in the appropriate library from this
# source tree.  To override this behavior (i.e. to force the test to
# use the libraries installed elsewhere on the system), set
# TEST_LD_LIBRARY_PATH and this script will not override it.
#
# For example, in a test directory, run:
#	TEST_LD_LIBRARY_PATH=\usr\lib .\TEST0
#
if (-Not $Env:TEST_TYPE_LD_LIBRARY_PATH) {
    switch -regex ($Env:BUILD) {
        'debug' { $Env:TEST_TYPE_LD_LIBRARY_PATH = '..\..\debug' }
        'nondebug' { $Env:TEST_TYPE_LD_LIBRARY_PATH = '..\..\nondebug' }
    }
}

#
# The variable DIR is constructed so the test uses that directory when
# constructing test files.  DIR is chosen based on the fs-type for
# this test, and if the appropriate fs-type doesn't have a directory
# defined in testconfig.sh, the test is skipped.
#
# This behavior can be overridden by passin in DIR with -d.  Example:
#	.\TEST0 -d \force\test\dir
#

sv -Name curtestdir (Get-Item -Path ".\").BaseName

# just in case
if (-Not $curtestdir) {
    Write-Error -Message "$curtestdir does not exist"
}

sv -Name curtestdir ("test_" + $curtestdir)

if (-Not $Env:UNITTEST_NUM) {
    throw "UNITTEST_NUM does not have a value"
}

if (-Not $Env:UNITTEST_NAME) {
    throw "UNITTEST_NAME does not have a value"
}

$Global:REAL_FS = $Env:FS

if ($DIR) {
    # if user passed it in...
    sv -Name DIR ($DIR + "\" + $curtestdir + $Env:UNITTEST_NUM)
} else {
    $tail = "\" + $Env:DIRSUFFIX + "\" + $curtestdir + $Env:UNITTEST_NUM
    # choose based on FS env variable
    switch ($Env:FS) {
        'pmem' {
            sv -Name DIR ($Env:PMEM_FS_DIR + $tail)
            if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
                $Env:PMEM_IS_PMEM_FORCE = "1"
            }
        }
        'non-pmem' {
             sv -Name DIR ($Env:NON_PMEM_FS_DIR + $tail)
        }
        'any' {
             if ($Env:PMEM_FS_DIR) {
                sv -Name DIR ($Env:PMEM_FS_DIR + $tail)
                $Global:REAL_FS='pmem'
                if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
                    $Env:PMEM_IS_PMEM_FORCE = "1"
                }
            } ElseIf ($Env:NON_PMEM_FS_DIR) {
                sv -Name DIR ($Env:NON_PMEM_FS_DIR + $tail)
                $Global:REAL_FS='non-pmem'
            } Else {
                throw "${Env:UNITTEST_NAME}: fs-type=any and both env vars are empty"
            }
        }
        'none' {
            sv -Name DIR "\nul\not_existing_dir\${curtestdir}${Env:UNITTEST_NUM}"
        }
        default {
            if (-Not $Env:UNITTEST_QUIET) {
                throw "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
            }
        }
    } # switch
}

if (isDir($Env:PMEM_FS_DIR)) {
    if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
        # "0" means there is PMEM
        $Global:PMEM_IS_PMEM = "0"
    } else {
        &$PMEMDETECT $Env:PMEM_FS_DIR
        $Global:PMEM_IS_PMEM = $Global:LASTEXITCODE
    }
}

if (isDir($Env:NON_PMEM_FS_DIR)) {
    &$PMEMDETECT $Env:NON_PMEM_FS_DIR
    $Global:NON_PMEM_IS_PMEM = $Global:LASTEXITCODE
}

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
$Env:VMEM_LOG_LEVEL = 3
$Env:VMEM_LOG_FILE = "vmem${Env:UNITTEST_NUM}.log"
$Env:PMEM_LOG_LEVEL = 3
$Env:PMEM_LOG_FILE = "pmem${Env:UNITTEST_NUM}.log"
$Env:PMEMBLK_LOG_LEVEL=3
$Env:PMEMBLK_LOG_FILE = "pmemblk${Env:UNITTEST_NUM}.log"
$Env:PMEMLOG_LOG_LEVE = 3
$Env:PMEMLOG_LOG_FILE = "pmemlog${Env:UNITTEST_NUM}.log"
$Env:PMEMOBJ_LOG_LEVEL = 3
$Env:PMEMOBJ_LOG_FILE= "pmemobj${Env:UNITTEST_NUM}.log"
$Env:PMEMPOOL_LOG_LEVEL = 3
$Env:PMEMPOOL_LOG_FILE= "pmempool${Env:UNITTEST_NUM}.log"

$Env:VMMALLOC_POOL_DIR = $DIR
$Env:VMMALLOC_POOL_SIZE = $((16 * 1024 * 1024))
$Env:VMMALLOC_LOG_LEVEL = 3
$Env:VMMALLOC_LOG_FILE = "vmmalloc${Env:UNITTEST_NUM}.log"

$Env:TRACE_LOG_FILE = "trace${Env:UNITTEST_NUM}.log"
$Env:ERR_LOG_FILE = "err${Env:UNITTEST_NUM}.log"
$Env:OUT_LOG_FILE = "out${Env:UNITTEST_NUM}.log"

$Env:MEMCHECK_LOG_FILE = "memcheck_${Env:BUILD}_${Env:UNITTEST_NUM}.log"
$Env:VALIDATE_MEMCHECK_LOG = 1

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
    $Env:UNITTEST_LOG_APPEND=1
}
