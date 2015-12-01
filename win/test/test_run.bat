@echo off

rem  Copyright (c) 2015, Intel Corporation
rem
rem  Redistribution and use in source and binary forms, with or without
rem  modification, are permitted provided that the following conditions
rem  are met:
rem
rem      * Redistributions of source code must retain the above copyright
rem         notice, this list of conditions and the following disclaimer.
rem
rem      * Redistributions in binary form must reproduce the above copyright
rem        notice, this list of conditions and the following disclaimer in
rem        the documentation and/or other materials provided with the
rem        distribution.
rem
rem      * Neither the name of Intel Corporation nor the names of its
rem        contributors may be used to endorse or promote products derived
rem        from this software without specific prior written permission.
rem
rem  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
rem  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
rem  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
rem  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
rem  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
rem  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
rem  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
rem  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
rem  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
rem  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
rem  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


set TEST_DIR=..\x64\Static-Debug
set POOL_FILE=.\testfile


del /F /Q %POOL_FILE%
fsutil file createnew %POOL_FILE% 16777216
%TEST_DIR%\obj_basic_integration %POOL_FILE%
echo ===========================================

%TEST_DIR%\obj_check %POOL_FILE%
echo ===========================================

del /F /Q %POOL_FILE%
fsutil file createnew %POOL_FILE% 16777216
%TEST_DIR%\obj_heap %POOL_FILE%
echo ===========================================

del /F /Q %POOL_FILE%
fsutil file createnew %POOL_FILE% 16777216
%TEST_DIR%\obj_heap_state %POOL_FILE%
echo ===========================================

del /F /Q %POOL_FILE%
fsutil file createnew %POOL_FILE% 16777216
%TEST_DIR%\obj_many_size_allocs %POOL_FILE%
echo ===========================================

del /F /Q %POOL_FILE%
fsutil file createnew %POOL_FILE% 16777216
%TEST_DIR%\obj_out_of_memory 1024 %POOL_FILE%
echo ===========================================

del /F /Q %POOL_FILE%

if "%1"=="p" pause
