/*
 * Copyright 2015-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * process_windows.c -- process creation for Windows
 */
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <Strsafe.h>
/*
 * so thi is not like really a fork at all but overloading for testing using
 * CreateProcess works just fine
 */
int
TestProcess(const char *path, int sleep)
{
	STARTUPINFO statusInfo;
	PROCESS_INFORMATION procInfo;
	TCHAR cmd[200] = TEXT("..\\..\\x64\\debug\\blk_pool_lock.exe ");
	TCHAR parm[100] = TEXT("");

	/* build the cmd to start a 2nd test process */
	int nChars = MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
	MultiByteToWideChar(CP_ACP, 0, path, -1, parm, nChars);
	_tcscat(cmd, parm);
	_tcscat(cmd, L" X");

	ZeroMemory(&statusInfo, sizeof(statusInfo));
	statusInfo.cb = sizeof(statusInfo);
	ZeroMemory(&procInfo, sizeof(procInfo));

	/* start the 2nd test process */
	if (!CreateProcess(NULL,
			cmd,
			NULL,
			NULL,
			FALSE,
			0,
			NULL,
			NULL,
			&statusInfo,
			&procInfo)) {
		return 0;
	}

	WaitForSingleObject(procInfo.hProcess, INFINITE);
	CloseHandle(procInfo.hProcess);
	CloseHandle(procInfo.hThread);
	return 1;
}
