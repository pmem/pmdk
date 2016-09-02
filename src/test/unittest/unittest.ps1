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

. "..\testconfig.ps1"

function touch {
    Out-File -InputObject $null -Encoding ascii -FilePath $args[0]
}

function cmp {
    fc.exe /b $args[0] $args[1] > $null
    if ($LASTEXITCODE -ne 0) {
        "$args differ"
    }
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
        Write-Error -Message 'No match: $args[0]'
        exit 1
    }
}

function getLineCount {
    [int64]$numLines = 0
    $buff = New-Object IO.StreamReader $args[0]
    while ($buff.ReadLine() -ne $null){ $numLines++ }
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
    for ($i=1;$i -lt $args.count;$i++) {
        # need to call out to sparsefile.exe to create a sparse file, note
        # that initial version of DAX doesn't support sparse
        $fname = $args[$i]
        & $SPARSEFILE $fname $size
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Error $LASTEXITCODE with sparsefile create"
            exit $LASTEXITCODE
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
# require_pmem -- only allow script to continue for a real PMEM device
#
function require_pmem {
    # note: PMEM_IS_PMEM 0 means it is PMEM, 1 means it is not
    if ($PMEM_IS_PMEM -eq "0") {
        return $true
    } Else {
        Write-Error "error: PMEM_FS_DIR=$Env:PMEM_FS_DIR does not point to a PMEM device"
        exit 1
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
#   psize - part size
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
#   and 32MB, and the replica with only one part of 48MB.  The first part file
#   is not created, the second is zeroed.  The only replica part is non-zeroed.
#   Also, the last file is read-only and its size does not match the information
#   from pool set file. The last line describes a remote replica.
#
#	create_poolset ./pool.set 16M:testfile1 32M:testfile2:z \
#				R 48M:testfile3:n:11M:0400 \
#				M remote_node:remote_pool.set
#
#
function create_poolset {
    $psfile = $args[0]
    echo "PMEMPOOLSET" | out-file -encoding ASCII $psfile
    for ($i=1;$i -lt $args.count;$i++) {
        if ($args[$i] -eq "M" -Or $args[$i] -eq 'm') { # remote replica
            $i++
            $cmd = $args[$i]
            $fparms = ($cmd.Split("{:}"))
            $node = $fparms[0]
            $desc = $fparms[1]
            echo "REPLICA $node $desc" | out-file -Append -encoding ASCII $psfile
            continue
        }
        if ($args[$i] -eq "R" -Or $args[$i] -eq 'r') {
            echo "REPLICA" | out-file -Append -encoding ASCII $psfile
            continue
        }
        $cmd = $args[$i]
        # need to strip out a drive letter if included because we use :
        # as a delimeter in the arguement

        $driveLetter = ""
        if ($cmd -match ":([a-zA-Z]):\\") {
            $tmp = ($cmd.Split("{:\\}",2,[System.StringSplitOptions]::RemoveEmptyEntries))
            $cmd = $tmp[0] + ":" + $tmp[1].SubString(2)
            $driveLetter = $tmp[1].SubString(0,2)
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

        if ($asize) {
            $asize = $asize -replace ".{1}$"
        } else {
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

        echo "$fsize $fpath" | out-file -Append -encoding ASCII $psfile
    } # for args


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

    Invoke-Expression "$command $params"

    check_exit_code
    # XXX: if we implement a memcheck thing... set some env vars here
}

#
# check_exit_code -- check if $LASTEXITCODE is equal 0
#
function check_exit_code {
 if ($LASTEXITCODE -ne 0) {
        sv -Name msg "failed with exit code $LASTEXITCODE"
        if (Test-Path ("err" + $Env:UNITTEST_NUM + ".log")) {
            if ($Env:UNITTEST_QUIET) {
                echo "${Env:UNITTEST_NAME}: $msg. err$Env:UNITTEST_NUM.log" >> ("err" + $Env:UNITTEST_NUM + ".log")
            } else {
                Write-Error "${Env:UNITTEST_NAME}: $msg. err$Env:UNITTEST_NUM.log"
            }
        } else {
            Write-Error "${Env:UNITTEST_NAME}: $msg"
        }

        # XXX: if we implement a memcheck thing...
        # if [ "$RUN_MEMCHECK" ]; then

        dump_last_n_lines out$Env:UNITTEST_NUM.log
        dump_last_n_lines $Env:PMEM_LOG_FILE
        dump_last_n_lines $Env:PMEMOBJ_LOG_FILE
        dump_last_n_lines $Env:PMEMLOG_LOG_FILE
        dump_last_n_lines $Env:PMEMBLK_LOG_FILE
        dump_last_n_lines $Env:VMEM_LOG_FILE
        dump_last_n_lines $Env:VMMALLOC_LOG_FILE

        #XXX:  bash just has a one-liner "false" here, does that
        # set the exit code?
        fail $LASTEXITCODE
    }
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

    Invoke-Expression "$command $params"
    if ($LASTEXITCODE -eq 0) {
        Write-Error "${Env:UNITTEST_NAME}: command succeeded unexpectedly."
    }
}

#
# check_pool -- run pmempool check on specified pool file
#
function check_pool {
    $file = $Args[0]
    if ($Env:CHECK_POOL -eq "1") {
        if ($Env:VERBOSE -ne "0") {
            echo "$Env:UNITTEST_NAME: checking consistency of pool $file"
        }
        Invoke-Expression "$PMEMPOOL$Env:EXESUFFIX check $file 2>&1 1>>$Env:CHECK_POOL_LOG_FILE"
        if ($LASTEXITCODE -ne 0) {
            Write-Error("$PMEMPOOL$Env:EXESUFFIX returned error code $LASTEXITCODE")
            Exit $LASTEXITCODE
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

    if ($Env:TEST -eq 'all') {
        return
    }

    for ($i=0;$i -lt $args.count;$i++) {
        if ($args[$i] -eq $Env:TEST) {
            return
        }
        switch ($Env:TEST) {
            'check' { # "check" is a synonym of "short + medium"
                if ($args[$i] -eq 'short' -Or $args[$i] -eq 'medium') {
                    return
                }
            }
            default {
                if ($args[$i] -eq $Env:TEST) {
                    return
                }
            }
        }
        if (-Not $Env:UNITTEST_QUIET) {
            echo "${Env:UNITTEST_NAME}: SKIP test-type $Env:TEST ($* required)"
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

        if (-Not $Env:UNITTEST_QUIET) {
            echo "${Env:UNITTEST_NAME}: SKIP build-type $Env:BUILD ($* required)"
        }
        exit 0
    }
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
# converts file to UTF8 w/o bom encoding
#
function convert_files_to_utf8_wo_bom {
    sv -Name files $args[0]
    foreach($file in $files) {
        $content = Get-Content $file
        $path = (Get-Item -Path ".\" -Verbose).FullName | Join-Path -ChildPath $file
        if($content -ne $null) {
            [IO.File]::WriteAllLines($path, $content)
        }
    }
}

#
# check -- check test results (using .match files)
#
# note: win32 version slightly different since the caller can't as
# easily bail when a cmd fails
#
function check {
    #	../match $(find . -regex "[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
    $perl = Get-Command -Name perl -ErrorAction SilentlyContinue
    If ($perl -eq $null) {
        Write-Error "Perl is missing, cannot check test results"
        fail 1
    }
    [string]$listing = Get-ChildItem -File | Where-Object  {$_.Name -match "[^0-9]${Env:UNITTEST_NUM}.log.match"}
    if ($listing) {
        $outputs = $listing.Split(' ')
        for($i=0; $i -lt $outputs.Count; $i++) {
            $outputs[$i] = ([io.fileinfo]$outputs[$i]).basename # remove .match extension
        }
        convert_files_to_utf8_wo_bom $outputs
        $pinfo = New-Object System.Diagnostics.ProcessStartInfo
        $pinfo.FileName = "perl"
        $pinfo.RedirectStandardError = $true
        $pinfo.RedirectStandardOutput = $true
        $pinfo.UseShellExecute = $false
        $pinfo.Arguments = "..\..\..\src\test\match $listing"
        $pinfo.WorkingDirectory = $PWD
        $p = New-Object System.Diagnostics.Process
        $p.StartInfo = $pinfo
        $p.Start() | Out-Null

        while($p.HasExited -eq $false) {
            # output streams have limited size, we need to read it
            # during an application runtime to prevent application hang.
            Write-Host -NoNewline $p.StandardError.ReadToEnd();
            Write-Host -NoNewline $p.StandardOutput.ReadToEnd();
        }

        if ($p.ExitCode -ne 0) {
            Write-Host -NoNewline $p.StandardError.ReadToEnd();
            Write-Host -NoNewline $p.StandardOutput.ReadToEnd();
            fail $p.ExitCode
        }
    } else {
        fail "No match file found for test $Env:UNITTEST_NAME"
    }

}

#
# pass -- print message that the test has passed
#
function pass {
    if ($Env:TM -eq 1) {
        sv -Name tm (Get-Date -Format G)
        $tm = "\t\t\t[$tm]"
    } else {
        sv -Name tm ""
    }

    sv -Name msg "PASS"
    Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
    Write-Host -NoNewline -foregroundcolor green $msg
    if ($tm) {
        Write-Host -NoNewline (":" + $tm)
    }

    if ($Env:FS -ne "none") {
        if (isDir $DIR) {
             rm -Force -Recurse $DIR
        }
    }
    Write-Host ""
}

#
# pass -- print message that the test has failed
#
function fail {
    sv -Name msg "FAILED"
    Write-Host -NoNewline ($Env:UNITTEST_NAME + ": ")
    Write-Host -NoNewLine -foregroundcolor red $msg
    Write-Host -NoNewline (" with errorcode " + $args[0])
    Write-Host -NoNewline (":" + $tm)
    Write-Host ""
    exit $args[0]
}

#
# check_file -- check if file exists and print error message if not
#
function check_file {
    if (-Not (Test-Path $Args[0])) {
        Write-Error "Missing File: " $Args[0]
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
        exit 1
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
# XXX: get_mode return diffrent value on appveyor
function check_mode {
    sv -Name mode -Scope "Local" $args[0]
    sv -Name file -Scope "Local" $args[1]
    sv -Name file_mode -Scope "Local" (get_mode $file)

    if ($file_mode -ne $mode) {
        Write-Error "error: wrong mode $file_mode != $mode"
        fail 1
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
    for ($i=0;$i -lt $args.count;$i+=2) {
        check_signature $args[$i] $args[$i+1]
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
    $enc = [System.Text.Encoding]::ASCII.GetString($buff)
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
    # XXX: not exactly the same as 'sed -e "/^UUID/,/^Checksum/d"'
    Invoke-Expression "$PMEMPOOL info $params" | `
        Select-String -notmatch -Pattern 'UUID' | `
        Select-String -notmatch -Pattern '^Checksum' | `
        Select-String -notmatch -Pattern '^Creation Time'
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
            [string]$params += -join(" '", $param, "' ")
        }
    }

    $rep1 = $args[$cnt + 1]
    $rep2 = $args[$cnt + 2]

    diff (dump_pool_info $params $rep1) (dump_pool_info $params $rep2)
}

#
# require_non_pmem -- only allow script to continue for a non-PMEM device
#
function require_non_pmem {
    if ($NON_PMEM_IS_PMEM -eq "1") {
        return $true
    } Else {
        Write-Error "error: NON_PMEM_FS_DIR=$Env:NON_PMEM_FS_DIR does not point to a non-PMEM device"
        exit 1
    }
}

#
# require_fs_type -- only allow script to continue for a certain fs type
#
function require_fs_type {
    sv -Name req_fs_type 1 -Scope Global
    for ($i=0;$i -lt $args.count;$i++) {
        if ($args[$i] -eq $Env:FS) {
            switch ($REAL_FS) {
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
# setup -- print message that test setup is commencing
#
function setup {
    $Env:LC_ALL = "C"

    # test type must be explicitly specified
    if ($req_test_type -ne "1") {
        Write-Error "error: required test type is not specified"
        exit 1
    }

    # fs type "none" must be explicitly enabled
    if ($Env:FS -eq "none" -and $req_fs_type -ne "1") {
        exit 0
    }

    # fs type "any" must be explicitly enabled
    if ($Env:FS -eq "any" -and $req_fs_type -ne "1") {
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

    Write-Host "${Env:UNITTEST_NAME}: SETUP ($Env:TEST\$REAL_FS\$Env:BUILD$MCSTR)"

    rm -Force check_pool_${Env:BUILD}_${Env:UNITTEST_NUM}.log -ErrorAction SilentlyContinue

    if ( $Env:FS -ne "none") {

        if (isDir $DIR) {
             rm -Force -Recurse $DIR
        }
        mkdir $DIR > $null
    }

    if ($TM -eq "1" ) { sv -Name start_time (epoch) }
}

function dump_last_n_lines {
    if (Test-Path $Args[0]) {
        sv -Name fname ((Get-Location).path + "\" + $Args[0])
        sv -Name ln (getLineCount $fname)
        if ($ln -gt $UT_DUMP_LINES) {
            Write-Error "Last $UT_DUMP_LINES lines of $fname below (whole file has $ln lines)."
        } else {
            Write-Error "$fname below."
        }

        Write-Host (Get-Content $fname -Tail $ln)
    }

}
#######################################################
#######################################################

# defaults
if (-Not $Env:TEST) { $Env:TEST = 'check'}
if (-Not $Env:FS) { $Env:FS = 'any'}
if (-Not $Env:BUILD) { $Env:BUILD = 'debug'}
if (-Not $Env:MEMCHECK) { $Env:MEMCHECK = 'auto'}
if (-Not $Env:CHECK_POOL) { $Env:CHECK_POOL = '0'}
if (-Not $Env:VERBOSE) { $Env:VERBOSE = '0'}
$Env:EXESUFFIX = ".exe"

if ($Env:EXE_DIR -eq $null) {
    $Env:EXE_DIR = "..\..\x64\debug"
}
$PMEMPOOL="$Env:EXE_DIR\pmempool"
$PMEMSPOIL="$Env:EXE_DIR\pmemspoil"
$PMEMWRITE="$Env:EXE_DIR\pmemwrite"
$PMEMALLOC="$Env:EXE_DIR\pmemalloc"
$PMEMDETECT="$Env:EXE_DIR\pmemdetect"

$SPARSEFILE="$Env:EXE_DIR\sparsefile"

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
if (-Not $Env:TEST_LD_LIBRARY_PATH) {
    switch -regex ($Env:BUILD) {
        'debug' { $Env:TEST_LD_LIBRARY_PATH = '..\..\debug' }
        'nondebug' { $Env:TEST_LD_LIBRARY_PATH = '..\..\nondebug' }
    }
}

#
# When running static binary tests, append the build type to the binary
#
#switch -wildcard ($Env:BUILD) {
#    'static-*' {$Env:EXESUFFIX = '.' + $Env:BUILD}
#}

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
    Write-Error "UNITTEST_NUM does not have a value"
    exit 1
}

if (-Not $Env:UNITTEST_NAME) {
    Write-Error "UNITTEST_NAME does not have a value"
    exit 1
}

sv -Name REAL_FS $Env:FS
if ($DIR) {
    # if user passed it in...
    sv -Name "DIR" ($DIR + "\" + $curtestdir + $Env:UNITTEST_NUM)
} else {
    $tail = "\" + $curtestdir + $Env:UNITTEST_NUM
    # choose based on FS env variable
    switch ($Env:FS) {
        'pmem' { sv -Name DIR ($Env:PMEM_FS_DIR + $tail)
                 if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
                     $Env:PMEM_IS_PMEM_FORCE = "1"
                 }
               }
        'non-pmem' { sv -Name DIR ($Env:NON_PMEM_FS_DIR + $tail) }
        'any' { if ($Env:PMEM_FS_DIR) {
                    sv -Name DIR ($Env:PMEM_FS_DIR + $tail)
                    $REAL_FS='pmem'
                    if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
                        $Env:PMEM_IS_PMEM_FORCE = "1"
                    }
                } ElseIf ($Env:NON_PMEM_FS_DIR) {
                    sv -Name DIR ($Env:NON_PMEM_FS_DIR + $tail)
                    $REAL_FS='non-pmem'
                } Else {
                    Write-Error "${Env:UNITTEST_NAME}: fs-type=any and both env vars are empty"
                    exit 1
                }
              }
        'none' {
            sv -Name DIR "/nul/not_existing_dir/${curtestdir}${Env:UNITTEST_NUM}" }
        default {
            if (-Not $Env:UNITTEST_QUIET) {
                Write-Host "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
                exit 0
            }
        }
    } # switch
}

if (isDir($Env:PMEM_FS_DIR)) {
    if ($Env:PMEM_FS_DIR_FORCE_PMEM -eq "1") {
        # "0" means there is PMEM
        $PMEM_IS_PMEM = "0"
    } else {
        &$PMEMDETECT $Env:PMEM_FS_DIR
        $PMEM_IS_PMEM = $LASTEXITCODE
    }
}

if (isDir($Env:NON_PMEM_FS_DIR)) {
    &$PMEMDETECT $Env:NON_PMEM_FS_DIR
    $NON_PMEM_IS_PMEM = $LASTEXITCODE
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

$Env:VMMALLOC_POOL_DIR = $DIR
$Env:VMMALLOC_POOL_SIZE = $((16 * 1024 * 1024))
$Env:VMMALLOC_LOG_LEVEL = 3
$Env:VMMALLOC_LOG_FILE = "vmmalloc${Env:UNITTEST_NUM}.log"

$Env:MEMCHECK_LOG_FILE = "memcheck_${Env:BUILD}_${Env:UNITTEST_NUM}.log"
$Env:VALIDATE_MEMCHECK_LOG = 1

if (-Not($UT_DUMP_LINES)) {
    sv -Name "UT_DUMP_LINES" 30
}

$Env:CHECK_POOL_LOG_FILE = "check_pool_${Env:BUILD}_${Env:UNITTEST_NUM}.log"
