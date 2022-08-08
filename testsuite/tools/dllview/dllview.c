// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * dllview.c -- a simple utility displaying the list of symbols exported by DLL
 *
 * usage: dllview filename
 */

#include <windows.h>
#include <stdio.h>
#include <winnt.h>
#include <imagehlp.h>
#include "util.h"

int
main(int argc, char *argv[])
{
	util_suppress_errmsg();
	if (argc < 2) {
		fprintf(stderr, "usage: %s dllname\n", argv[0]);
		exit(1);
	}

	const char *dllname = argv[1];

	LOADED_IMAGE img;
	if (MapAndLoad(dllname, NULL, &img, 1, 1) == FALSE) {
		fprintf(stderr, "cannot load DLL image\n");
		exit(2);
	}

	IMAGE_EXPORT_DIRECTORY *dir;
	ULONG dirsize;
	dir = (IMAGE_EXPORT_DIRECTORY *)ImageDirectoryEntryToData(
			img.MappedAddress, 0 /* mapped as image */,
			IMAGE_DIRECTORY_ENTRY_EXPORT, &dirsize);
	if (dir == NULL) {
		fprintf(stderr, "cannot read image directory\n");
		UnMapAndLoad(&img);
		exit(3);
	}

	DWORD *rva;
	rva = (DWORD *)ImageRvaToVa(img.FileHeader, img.MappedAddress,
			dir->AddressOfNames, NULL);

	for (DWORD i = 0; i < dir->NumberOfNames; i++) {
		char *name = (char *)ImageRvaToVa(img.FileHeader,
				img.MappedAddress, rva[i], NULL);
		printf("%s\n", name);
	}

	UnMapAndLoad(&img);
	return 0;
}
