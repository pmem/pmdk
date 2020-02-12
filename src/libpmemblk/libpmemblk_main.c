// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2017, Intel Corporation */

/*
 * libpmemblk_main.c -- entry point for libpmemblk.dll
 *
 * XXX - This is a placeholder.  All the library initialization/cleanup
 * that is done in library ctors/dtors, as well as TLS initialization
 * should be moved here.
 */

void libpmemblk_init(void);
void libpmemblk_fini(void);

int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		libpmemblk_init();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		libpmemblk_fini();
		break;
	}
	return TRUE;
}
