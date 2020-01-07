// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * libpmemlog_main.c -- entry point for libpmemlog.dll
 *
 * XXX - This is a placeholder.  All the library initialization/cleanup
 * that is done in library ctors/dtors, as well as TLS initialization
 * should be moved here.
 */

void libpmemlog_init(void);
void libpmemlog_fini(void);

int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason) {
	case DLL_PROCESS_ATTACH:
		libpmemlog_init();
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;

	case DLL_PROCESS_DETACH:
		libpmemlog_fini();
		break;
	}
	return TRUE;
}
