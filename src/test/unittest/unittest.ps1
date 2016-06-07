#
# Copyright 2015-2016, Intel Corporation
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

function epoch {
    return [int64](([datetime]::UtcNow)-(get-date "1/1/1970")).TotalMilliseconds
}

function isDir {
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
# create_file -- create zeroed out files of a given length in megs
#
# example, to create two files, each 1GB in size:
#	create_file 1024 testfile1 testfile2
#
# Note: this literally fills the file with 0's to make sure its
# not a sparse file.  Its slow but the fastest method I could find
#
# Input unit size is MB
#
function create_file {
    sv -Name size $args[0]
    [int64]$size *= 1024 * 1024
    for ($i=1;$i -lt $args.count;$i++) {
        $stream = new-object system.IO.StreamWriter($args[$i], "False", [System.Text.Encoding]::Ascii)
        1..$size | %{ $stream.Write("0") }
        $stream.close()
        Get-ChildItem $args[$i]* >> ("prep" + $Env:UNITTEST_NUM + ".log")
    }
}
#
# create_holey_file -- create holey files of a given length in megs
#
# example, to create two files, each 1GB in size:
#	create_holey_file 1024 testfile1 testfile2
#
# Input unit size is MB (unless a string is passed in then its mMB+nKB)
#
function create_holey_file {
    sv -Name size $args[0]
    if ($size -is "String" -And $size.contains(“+”)) {
        # for tests that want to pass in a combo of MB+KB
        [int64]$MB = $size.split("+")[0]
        [int64]$KB = $size.split("+")[1]
        [int64]$size = $MB * 1024 * 1024
        $size += $KB * 1024
    } else {
        [int64]$size *= 1024 * 1024
    }
    for ($i=1;$i -lt $args.count;$i++) {
        # need to call out to sparsefile.exe to create a sparse file, note
        # that initial version of DAX doesn't support sparse
        $fname = $args[$i]
        & '..\..\x64\debug\sparsefile.exe' $fname $size
        if ($LASTEXITCODE -ne 0) {
            Write-Error "Error $LASTEXITCODE with sparsefile create"
            exit $LASTEXITCODE
        }
        Get-ChildItem $fname >> ("prep" + $Env:UNITTEST_NUM + ".log")
    }
}

#
# create_nonzeroed_file -- create non-zeroed files of a given length in megs
#
# A given first kilobytes of the file is zeroed out.
#
# example, to create two files, each 1GB in size, with first 4K zeroed
#	create_nonzeroed_file 1024 4 testfile1 testfile2
#
# Note: from 0 to offset is sparse, after that filled with Z
# Input unit size is MB for file size KB for offset
#
function create_nonzeroed_file {
    sv -Name offset $args[1]
    $offset *= 1024
    sv -Name size $args[0]
    [int64]$size = $(([int64]$size * 1024 * 1024  - $offset))
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
    if ($Env:PMEM_IS_PMEM) {
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
# replica set.
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
# example:
#   The following command define a pool set consisting of two parts: 16MB
#   and 32MB, and the replica with only one part of 48MB.  The first part file
#   is not created, the second is zeroed.  The only replica part is non-zeroed.
#   Also, the last file is read-only and its size does not match the information
#   from pool set file.
#
#	create_poolset ./pool.set 16M:testfile1 32M:testfile2:z \
#				R 48M:testfile3:n:11M:0400
#
function create_poolset {
    sv -Name psfile $args[0]
    echo "PMEMPOOLSET" | out-file -encoding ASCII $psfile
    for ($i=1;$i -lt $args.count;$i++) {
        if ($args[$i] -eq "R" -Or $args[$i] -eq 'r') {
            echo "REPLICA" | out-file -Append -encoding ASCII $psfile
            continue
        }
        sv -Name cmd $args[$i]
        # need to strip out a drive letter if included because we use :
        # as a delimeter in the arguement
        sv -Name driveLetter ""
        if ($cmd -match "([a-zA-Z]):\\") {
            sv -Name tmp ($cmd.Split("{:\\}",2,[System.StringSplitOptions]::RemoveEmptyEntries))
            $cmd = $tmp[0] + ":" + $tmp[1].SubString(2)
            $driveLetter = $tmp[1].SubString(0,2)
        }
        sv -Name fparms ($cmd.Split("{:}"))
        sv -Name fsize $fparms[0]

        # XXX: unclear how to follow a symlink
        # like linux "fpath=`readlink -mn ${fparms[1]}`" but I've not tested
        # that it works with a symlink or shortcut
        sv -Name fpath $fparms[1]
        if (-Not $driveLetter -eq "") {
            $fpath = $driveLetter + $fpath
        }
        sv -Name cmd $fparms[2]
        sv -Name asize $fparms[3]
        sv -Name mode $fparms[4]

        if ($asize) {
            $asize = $asize -replace ".{1}$"
        } else {
            sv -Name asize $fsize
        }

        [int64] $asize *= 1024 * 1024
        [int64] $fsize *= 1024 * 1024

        switch -regex ($cmd) {
            # do nothing
            'x' { }
            # zeroed (holey) file
            'z' { create_holey_file $asize $fpath }
            # non-zeroed file
            'n' { create_file $asize $fpath }
            # non-zeroed file, except 4K header
            'h' { create_nonzeroed_file $asize 4 $fpath }
        }
        # XXX: didn't convert chmod
        #	if [ $mode ]; then
        #	    chmod $mode $fpath
        #	fi

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

    [string]$expression =  @($Args)
    #$expression = $expression -replace " ", " ; "
    Invoke-Expression $expression
    sv -Name ret $?

    if (-Not $ret) {
        sv -Name msg "failed with exit code FALSE"

        if (Test-Path ("err" + $Env:UNITTEST_NUM + ".log")) {
            if ($Env:UNITTEST_QUIET) {
                echo "${Env:UNITTEST_NAME}: $msg. err$Env:UNITTEST_NUM.log" >> ("err" + $Env:UNITTEST_NUM + ".log")
            } else {
                Write-Error "${Env:UNITTEST_NAME}: $msg. err$Env:UNITTEST_NUM.log"
            }
        } else {
            Write-Error "${Env:UNITTEST_NAME}: $msg"
        }

        # XXX: if we impement a memcheck thing...
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
    }

    # XXX: if we impement a memcheck thing... set some env vars here
}

#
# expect_abnormal_exit -- run a given command, expect it to exit non-zero
#
function expect_abnormal_exit {
    #XXX:  bash sets up LD_PRELOAD and other gcc options here
    # that we can't do, investigating how to address API hooking...

    [string]$expression =  @($Args)
    $expression = $expression -replace " ", " ; "
    Invoke-Expression $expression
    sv -Name ret $?

    if ($ret) {
        sv -Name msg "succeeded"
        Write-Error "${Env:UNITTEST_NAME}: command $msg unexpectedly."
        #XXX:  bash just has a one-liner "false" here, does that
        # set the exit code?
    }
}

#
# check_pool -- run pmempool check on specified pool file
#
function check_pool {
    # XXX - - tool not available on windows yet
    Write-Host "function check_pool() Not yet implemented"
}

#
# check_pools -- run pmempool check on specified pool files
#
function check_pools {
    # XXX - tool not available on windows yet
    Write-Host "function check_pools() Not yet implemented"
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
}

#
# require_no_superuser -- require user without superuser rights
#
# XXX: not sure how to translate
#
function require_no_superuser {
    Write-Host "${Env:UNITTEST_NAME}: SKIP required: run without superuser rightsy"
    exit 0
}

#
# require_test_type -- only allow script to continue for a certain test type
#
function require_test_type() {
    for ($i=0;$i -lt $args.count;$i++) {
        if ($args[$i] -eq $Env:TEST) {
            return
        }
        #XXX look at the bash code w/someone and confirm the logic here
        if (! $Env:UNITTEST_QUIET) {
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
        #XXX look at the bash code w/someone and confirm the logic here
        if (! $Env:UNITTEST_QUIET) {
            echo "${Env:UNITTEST_NAME}: SKIP build-type $Env:BUILD ($* required)"
        }
        exit 0
    }
}

#
# require_pkg -- only allow script to continue if specified package exists
#
function require_pkg {
    # XXX: placeholder for checking dependencies if we can
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
       if (! $Env:UNITTEST_QUIET) {
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
    #	../match $(find . -regex "[^0-9]*${UNITTEST_NUM}\.log\.match" | xargs)
    [string]$listing = Get-ChildItem -File | Where-Object  {$_.Name -match "[^0-9]${Env:UNITTEST_NUM}.log.match"}
    if ($listing) {
        $p = start-process -PassThru -Wait -NoNewWindow -FilePath perl -ArgumentList '..\..\..\src\test\match', $listing
        if ($p.ExitCode -eq 0) {
            pass
        } else {
            fail $p.ExitCode
        }
    } else {
        fail "No match file found for test $Env:UNITTEST_NAME"
    }
    Write-Host ""

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
        exit 1
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
        return $false
    }
    return $true
}

#
# check_mode -- validate file mode
#
function check_mode {
    sv -Name mode -Scope "Local" $args[0]
    sv -Name file -Scope "Local" $args[1]
    sv -Name file_mode -Scope "Local" (get_mode $file)

    if ($file_mode -ne $mode) {
        Write-Error "error: wrong mode $file_mode != $mode"
        return $false
    }
    return $true
}

#
# check_signature -- check if file contains specified signature
#
function check_signature {
    sv -Name sig -Scope "Local" $args[0]
    sv -Name file -Scope "Local" ((Get-Location).path + "\" + $Args[1])
    sv -Name file_sig -Scope "Local" ""
    $stream = [System.IO.File]::OpenRead($file)
    $buff = New-Object Byte[] $SIG_LEN
    $stream.Read($buff, 0, $SIG_LEN)
    $file_sig = [System.Text.Encoding]::Ascii.GetString($buff)
    if ($file_sig -ne $sig) {
        Write-Error "error: $file signature doesn't match $file_sig != $sig"
        return $false
    }
    return $true
}

#
# check_signatures -- check if multiple files contain specified signature
#
function check_signatures {
	for ($i=0;$i -lt $args.count;$i+=2) {
        if (-Not (check_signature $args[$i] $args[$i+1])) {
            return $false
        }
    }
    return $true
}

#
# check_layout -- check if pmemobj pool contains specified layout
#
function check_layout {
    sv -Name layout -Scope "Local" $args[0]
    sv -Name file -Scope "Local" ((Get-Location).path + "\" + $Args[1])

    # XXX: not fully tested
    $stream = [System.IO.File]::OpenRead($file)
    $stream.Position = $LAYOUT_OFFSET
    $buff = New-Object Byte[] $LAYOUT_LEN
    $stream.Read($buff, 0, $LAYOUT_LEN)

    if ($buff -ne $layout) {
        Write-Error "error: layout doesn't match $buff != $layout"
        return $false
    }
    return $true
}

#
# check_arena -- check if file contains specified arena signature
#
function check_arena {
    sv -Name file -Scope "Local" ((Get-Location).path + "\" + $Args[0])

    # XXX: not fully tested
    $stream = [System.IO.File]::OpenRead($file)
    $stream.Position = $ARENA_OFFSET
    $buff = New-Object Byte[] $ARENA_SIG_LEN
    $stream.Read($buff, 0, $ARENA_SIG_LEN)

    if ($buff -ne $ARENA_SIG) {
        Write-Error "error: can't find arena signature"
        return $false
    }
    return $true
}

#
# dump_pool_info -- dump selected pool metadata and/or user data
#
function dump_pool_info {
    #XXX: not yet implemented
    Write-Error "function dump_pool_info() not yet implemented"
    # ignore selected header fields that differ by definition
    #${PMEMPOOL}.static-nondebug info $* | sed -e "/^UUID/,/^Checksum/d"
}

#
# compare_replicas -- check replicas consistency by comparing `pmempool info` output
#
function compare_replicas {
    Write-Error "function compare_replicas() not yet implemented"
    #diff <(dump_pool_info $1 $2) <(dump_pool_info $1 $3)
}

#
# require_non_pmem -- only allow script to continue for a non-PMEM device
#
function require_non_pmem {
    if ($Env:NON_PMEM_IS_PMEM) {
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
    if (! $Env:UNITTEST_QUIET) {
        Write-Host "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
    }
    exit 0
}

#
# setup -- print message that test setup is commencing
#
function setup {
    $Env:LC_ALL = "C"

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
        # bla, ask Andy what this does exactly (format wise this wil likely be a PITA)
        # paste -d " " <(yes $UNITTEST_NAME $1 | head -n $ln) <(tail -n $ln $1) >&2
        Write-Host (Get-Content $fname -Tail $ln)
    }

}
#######################################################
#######################################################

# defaults
if (! $Env:TEST) { $Env:TEST = 'check'}
if (! $Env:FS) { $Env:FS = 'any'}
if (! $Env:BUILD) { $Env:BUILD = 'debug'}
if (! $Env:MEMCHECK) { $Env:MEMCHECK = 'auto'}
if (! $Env:CHECK_POOL) { $Env:CHECK_POOL = '0'}
if (! $Env:VERBOSE) { $Env:VERBOSE = '0'}
$Env:EXESUFFIX = ".exe"

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
if (! $Env:TEST_LD_LIBRARY_PATH) {
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
if (! $curtestdir) {
    Write-Error -Message "$curtestdir does not exist"
}

sv -Name curtestdir ("test_" + $curtestdir)

if (! $Env:UNITTEST_NUM) {
    Write-Error "UNITTEST_NUM does not have a value"
    exit 1
}

if (! $Env:UNITTEST_NAME) {
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
        'pmem' { sv -Name DIR ($PMEM_FS_DIR + $tail)
                 if ($PMEM_FS_DIR_FORCE_PMEM) {
                     $Env:PMEM_IS_PMEM_FORCE = 1
                 }
               }
        'non-pmem' { sv -Name DIR ($NON_PMEM_FS_DIR + $tail) }
        'any' { if ($PMEM_FS_DIR) {
                    sv -Name DIR ($PMEM_FS_DIR + $tail)
                    $REAL_FS='pmem'
                    if ($PMEM_FS_DIR_FORCE_PMEM) {
                        $Env:PMEM_IS_PMEM_FORCE = 1
                    }
                } ElseIf ($NON_PMEM_FS_DIR) {
                    sv -Name DIR ($NON_PMEM_FS_DIR + $tail)
                    $REAL_FS='non-pmem'
                } Else {
                    Write-Error "${Env:UNITTEST_NAME}: fs-type=any and both env vars are empty"
                    exit 1
                }
              }
        'none' {
            sv -Name DIR "/nul/not_existing_dir/${curtestdir}${Env:UNITTEST_NUM}" }
        default {
            if (! $Env:UNITTEST_QUIET) {
                Write-Host "${Env:UNITTEST_NAME}: SKIP fs-type $Env:FS (not configured)"
                exit 0
            }
        }
    } # switch
}

# XXX REMOVE THIS WHEN ITS ALL WORKING
if (! $DIR) {
    Write-Error -Message 'DIR does not exist'
    exit 1
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

if (! $UT_DUMP_LINES) {
    sv -Name "UT_DUMP_LINES" 30
}

$Env:CHECK_POOL_LOG_FILE = "check_pool_${Env:BUILD}_${Env:UNITTEST_NUM}.log"
