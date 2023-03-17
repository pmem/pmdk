/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * output.h -- declarations of output printing related functions
 */

#include <time.h>
#include <stdint.h>
#include <stdio.h>

#define BLK_DEPR_STR "Libpmemblk is deprecated."
#ifdef _WIN32
#define PMEMBLK_DEPR_ATTR __declspec(deprecated(BLK_DEPR_STR))

#define WIN_DEPR_STR "Windows support is deprecated."
#define WIN_DEPR_ATTR __declspec(deprecated(WIN_DEPR_STR))
#else
#define PMEMBLK_DEPR_ATTR __attribute__((deprecated(BLK_DEPR_STR)))
#endif

#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void out_set_vlevel(int vlevel);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void out_set_stream(FILE *stream);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void out_set_prefix(const char *prefix);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void out_set_col_width(unsigned col_width);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_err(const char *fmt, ...) FORMAT_PRINTF(1, 2);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void out_err(const char *file, int line, const char *func,
		const char *fmt, ...) FORMAT_PRINTF(4, 5);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_err_vargs(const char *fmt, va_list ap);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_indent(int vlevel, int i);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv(int vlevel, const char *fmt, ...) FORMAT_PRINTF(2, 3);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_nl(int vlevel);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
int outv_check(int vlevel);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_title(int vlevel, const char *fmt, ...) FORMAT_PRINTF(2, 3);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_field(int vlevel, const char *field, const char *fmt,
		...) FORMAT_PRINTF(3, 4);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
void outv_hexdump(int vlevel, const void *addr, size_t len, size_t offset,
		int sep);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_uuid_str(uuid_t uuid);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_time_str(time_t time);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_size_str(uint64_t size, int human);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_percentage(double percentage);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_checksum(void *addr, size_t len, uint64_t *csump,
		uint64_t skip_off);
PMEMBLK_DEPR_ATTR const char *out_get_btt_map_entry(uint32_t map);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_pool_type_str(pmem_pool_type_t type);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_pool_signature(pmem_pool_type_t type);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_tx_state_str(uint64_t state);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_chunk_type_str(enum chunk_type type);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_chunk_flags(uint16_t flags);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_zone_magic_str(uint32_t magic);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_pmemoid_str(PMEMoid oid, uint64_t uuid_lo);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_arch_machine_class_str(uint8_t machine_class);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_arch_data_str(uint8_t data);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_arch_machine_str(uint16_t machine);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_last_shutdown_str(uint8_t dirty);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_alignment_desc_str(uint64_t ad, uint64_t cur_ad);
#ifdef _WIN32
WIN_DEPR_ATTR
#endif
const char *out_get_incompat_features_str(uint32_t incompat);
