// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * libpmem_main.c -- entry point for libpmem.dll
 *
 * XXX - This is a placeholder.  All the library initialization/cleanup
 * that is done in library ctors/dtors, as well as TLS initialization
 * should be moved here.
 */

void libpmem_init(void);
void libpmem_fini(void);

int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		libpmem_init();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		libpmem_fini();
		break;
	}
	return TRUE;
}
