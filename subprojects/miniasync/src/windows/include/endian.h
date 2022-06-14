/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * endian.h -- convert values between host and big-/little-endian byte order
 */

#ifndef ENDIAN_H
#define ENDIAN_H 1

/*
 * XXX: On Windows we can assume little-endian architecture
 */
#include <intrin.h>

#define htole16(a) (a)
#define htole32(a) (a)
#define htole64(a) (a)

#define le16toh(a) (a)
#define le32toh(a) (a)
#define le64toh(a) (a)

#define htobe16(x) _byteswap_ushort(x)
#define htobe32(x) _byteswap_ulong(x)
#define htobe64(x) _byteswap_uint64(x)

#define be16toh(x)  _byteswap_ushort(x)
#define be32toh(x)  _byteswap_ulong(x)
#define be64toh(x)  _byteswap_uint64(x)

#endif /* ENDIAN_H */
