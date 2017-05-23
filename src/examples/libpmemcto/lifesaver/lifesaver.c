/*
 * Copyright 2017, Intel Corporation
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
 * lifesaver.c -- a simple screen saver which implements Conway's Game of Life
 */

#include <windows.h>
#include <scrnsave.h>
#include <time.h>
#include <stdlib.h>

#include <libpmemcto.h>

#include "life.h"
#include "resource.h"

#define DEFAULT_PATH "c:\\temp\\life.cto"
#define TIMER_ID 1

CHAR Path[MAX_PATH];

CHAR szAppName[] = "Life's Screen-Saver";
CHAR szIniFile[] = "lifesaver.ini";
CHAR szParamPath[] = "Data file path";

static int Width;
static int Height;

/*
 * game_draw -- displays game board
 */
void
game_draw(HWND hWnd, struct game *gp)
{
	RECT rect;
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hWnd, &ps);

	HBITMAP bmp = CreateBitmap(gp->width, gp->height, 1, 8, gp->board);
	HBRUSH brush = CreatePatternBrush(bmp);
	GetWindowRect(hWnd, &rect);
	FillRect(hdc, &rect, brush);
	DeleteObject(brush);
	DeleteObject(bmp);

	EndPaint(hWnd, &ps);
}

/*
 * load_config -- loads screen saver config
 */
static void
load_config(void)
{
	DWORD res = GetPrivateProfileString(szAppName, szParamPath,
			DEFAULT_PATH, Path, sizeof(Path), szIniFile);
}

/*
 * ScreenSaverConfigureDialog -- handles configuration dialog window
 *
 * XXX: fix saving configuration
 */
BOOL WINAPI
ScreenSaverConfigureDialog(HWND hDlg, UINT message,
	WPARAM wParam, LPARAM lParam)
{
	static HWND hOK;
	static HWND hPath;
	HRESULT hr;
	BOOL res;

	switch (message) {
	case WM_INITDIALOG:
		load_config();
		hPath = GetDlgItem(hDlg, IDC_PATH);
		hOK = GetDlgItem(hDlg, IDC_OK);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_OK:
			/* write current file path to the .ini file */
			res = WritePrivateProfileString(szAppName,
					szParamPath, Path, szIniFile);

			/* fall-thru to EndDialog() */

		case IDC_CANCEL:
			EndDialog(hDlg, LOWORD(wParam) == IDC_OK);

		return TRUE;
		}
	}

	return FALSE;
}

BOOL
RegisterDialogClasses(HANDLE hInst)
{
	return TRUE;
}

/*
 * ScreenSaverProc -- screen saver window proc
 */
LRESULT CALLBACK
ScreenSaverProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	static HANDLE hTimer;
	static struct game *gp;

	HDC hdc;
	static RECT rc;

	switch (iMessage) {
	case WM_CREATE:
		Width = GetSystemMetrics(SM_CXSCREEN);
		Height = GetSystemMetrics(SM_CYSCREEN);
		load_config();
		gp = game_init(Path, Width, Height, 10);
		hTimer = (HANDLE)SetTimer(hWnd, TIMER_ID, 10, NULL); /* 10ms */
		return 0;

	case WM_ERASEBKGND:
		hdc = GetDC(hWnd);
		GetClientRect(hWnd, &rc);
		FillRect(hdc, &rc, GetStockObject(BLACK_BRUSH));
		ReleaseDC(hWnd, hdc);
		return 0;

	case WM_TIMER:
		game_next(gp);
		InvalidateRect(hWnd, NULL, FALSE);
		return 0;

	case WM_PAINT:
		game_draw(hWnd, gp);
		return 0;

	case WM_DESTROY:
		if (hTimer)
			KillTimer(hWnd, TIMER_ID);
		pmemcto_close(gp->pcp);
		PostQuitMessage(0);
		return 0;
	}

	return (DefScreenSaverProc(hWnd, iMessage, wParam, lParam));
}
