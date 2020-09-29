// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * libpmemset_main.c -- entry point for libpmemset.dll
 */

void libpmemset_init(void);
void libpmemset_fini(void);

int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		libpmemset_init();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		libpmemset_fini();
		break;
	}
	return TRUE;
}
