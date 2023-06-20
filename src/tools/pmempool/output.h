/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2023, Intel Corporation */

/*
 * output.h -- declarations of output printing related functions
 */

#include <time.h>
#include <stdint.h>
#include <stdio.h>

void out_set_vlevel(int vlevel);
void out_set_stream(FILE *stream);
void out_set_prefix(const char *prefix);
void out_set_col_width(unsigned col_width);
void outv_err(const char *fmt, ...) FORMAT_PRINTF(1, 2);
void out_err(const char *file, int line, const char *func,
		const char *fmt, ...) FORMAT_PRINTF(4, 5);
void outv_err_vargs(const char *fmt, va_list ap);
void outv_indent(int vlevel, int i);
void outv(int vlevel, const char *fmt, ...) FORMAT_PRINTF(2, 3);
void outv_nl(int vlevel);
int outv_check(int vlevel);
void outv_title(int vlevel, const char *fmt, ...) FORMAT_PRINTF(2, 3);
void outv_field(int vlevel, const char *field, const char *fmt,
		...) FORMAT_PRINTF(3, 4);
void outv_hexdump(int vlevel, const void *addr, size_t len, size_t offset,
		int sep);
const char *out_get_uuid_str(uuid_t uuid);
const char *out_get_time_str(time_t time);
const char *out_get_size_str(uint64_t size, int human);
const char *out_get_percentage(double percentage);
const char *out_get_checksum(void *addr, size_t len, uint64_t *csump,
		uint64_t skip_off);
const char *out_get_pool_type_str(pmem_pool_type_t type);
const char *out_get_pool_signature(pmem_pool_type_t type);
const char *out_get_tx_state_str(uint64_t state);
const char *out_get_chunk_type_str(enum chunk_type type);
const char *out_get_chunk_flags(uint16_t flags);
const char *out_get_zone_magic_str(uint32_t magic);
const char *out_get_pmemoid_str(PMEMoid oid, uint64_t uuid_lo);
const char *out_get_arch_machine_class_str(uint8_t machine_class);
const char *out_get_arch_data_str(uint8_t data);
const char *out_get_arch_machine_str(uint16_t machine);
const char *out_get_last_shutdown_str(uint8_t dirty);
const char *out_get_alignment_desc_str(uint64_t ad, uint64_t cur_ad);
const char *out_get_incompat_features_str(uint32_t incompat);
