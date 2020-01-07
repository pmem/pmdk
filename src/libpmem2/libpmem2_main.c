// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * libpmem2_main.c -- entry point for libpmem2.dll
 */

void libpmem2_init(void);
void libpmem2_fini(void);

int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		libpmem2_init();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		libpmem2_fini();
		break;
	}
	return TRUE;
}
