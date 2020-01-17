// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of dimm functions
 */

#ifndef WRAP_REAL
#define pmem2_get_device_uscU __wrap_pmem2_get_device_usc
#define pmem2_get_device_idU __wrap_pmem2_get_device_id
#endif
