// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

/*
 * output.c -- definitions of output printing related functions
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <err.h>
#include <endian.h>
#include <inttypes.h>
#include <float.h>
#include "feature.h"
#include "common.h"
#include "output.h"

#define _STR(s)	#s
#define STR(s) _STR(s)
#define TIME_STR_FMT "%a %b %d %Y %H:%M:%S"
#define UUID_STR_MAX 37
#define HEXDUMP_ROW_WIDTH 16
/*
 * 2 chars + space per byte +
 * space after 8 bytes and terminating NULL
 */
#define HEXDUMP_ROW_HEX_LEN (HEXDUMP_ROW_WIDTH * 3 + 1 + 1)
/* 1 printable char per byte + terminating NULL */
#define HEXDUMP_ROW_ASCII_LEN (HEXDUMP_ROW_WIDTH + 1)
#define SEPARATOR_CHAR '-'
#define MAX_INDENT 32
#define INDENT_CHAR ' '

static char out_indent_str[MAX_INDENT + 1];
static int out_indent_level;
static int out_vlevel;
static unsigned out_column_width = 20;
static FILE *out_fh;
static const char *out_prefix;

#define STR_MAX 256

/*
 * outv_check -- verify verbosity level
 */
int
outv_check(int vlevel)
{
	return vlevel && (out_vlevel >= vlevel);
}

/*
 * out_set_col_width -- set column width
 *
 * See: outv_field() function
 */
void
out_set_col_width(unsigned col_width)
{
	out_column_width = col_width;
}

/*
 * out_set_vlevel -- set verbosity level
 */
void
out_set_vlevel(int vlevel)
{
	out_vlevel = vlevel;
	if (out_fh == NULL)
		out_fh = stdout;
}

/*
 * out_set_prefix -- set prefix to output format
 */
void
out_set_prefix(const char *prefix)
{
	out_prefix = prefix;
}

/*
 * out_set_stream -- set output stream
 */
void
out_set_stream(FILE *stream)
{
	out_fh = stream;

	memset(out_indent_str, INDENT_CHAR, MAX_INDENT);
}

/*
 * outv_err -- print error message
 */
void
outv_err(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	outv_err_vargs(fmt, ap);
	va_end(ap);
}

/*
 * outv_err_vargs -- print error message
 */
void
outv_err_vargs(const char *fmt, va_list ap)
{
	char *_str = strdup(fmt);
	if (!_str)
		err(1, "strdup");
	char *str = _str;

	fprintf(stderr, "error: ");
	int errstr = str[0] == '!';
	if (errstr)
		str++;

	char *nl = strchr(str, '\n');
	if (nl)
		*nl = '\0';

	vfprintf(stderr, str, ap);
	if (errstr)
		fprintf(stderr, ": %s", strerror(errno));
	fprintf(stderr, "\n");

	free(_str);
}

/*
 * outv_indent -- change indentation level by factor
 */
void
outv_indent(int vlevel, int i)
{
	if (!outv_check(vlevel))
		return;

	out_indent_str[out_indent_level] = INDENT_CHAR;
	out_indent_level += i;
	if (out_indent_level < 0)
		out_indent_level = 0;
	if (out_indent_level > MAX_INDENT)
		out_indent_level = MAX_INDENT;

	out_indent_str[out_indent_level] = '\0';
}

/*
 * _out_prefix -- print prefix if defined
 */
static void
_out_prefix(void)
{
	if (out_prefix)
		fprintf(out_fh, "%s: ", out_prefix);
}

/*
 * _out_indent -- print indent
 */
static void
_out_indent(void)
{
	fprintf(out_fh, "%s", out_indent_str);
}

/*
 * outv -- print message taking into account verbosity level
 */
void
outv(int vlevel, const char *fmt, ...)
{
	va_list ap;

	if (!outv_check(vlevel))
		return;

	_out_prefix();
	_out_indent();
	va_start(ap, fmt);
	vfprintf(out_fh, fmt, ap);
	va_end(ap);
}

/*
 * outv_nl -- print new line without indentation
 */
void
outv_nl(int vlevel)
{
	if (!outv_check(vlevel))
		return;

	_out_prefix();
	fprintf(out_fh, "\n");
}

void
outv_title(int vlevel, const char *fmt, ...)
{
	va_list ap;
	if (!outv_check(vlevel))
		return;

	fprintf(out_fh, "\n");
	_out_prefix();
	_out_indent();
	va_start(ap, fmt);
	vfprintf(out_fh, fmt, ap);
	va_end(ap);
	fprintf(out_fh, ":\n");
}

/*
 * outv_field -- print field name and value in specified format
 *
 * Field name will have fixed width which can be changed by
 * out_set_column_width() function.
 * vlevel - verbosity level
 * field  - field name
 * fmt    - format form value
 */
void
outv_field(int vlevel, const char *field, const char *fmt, ...)
{
	va_list ap;

	if (!outv_check(vlevel))
		return;

	_out_prefix();
	_out_indent();
	va_start(ap, fmt);
	fprintf(out_fh, "%-*s : ", out_column_width, field);
	vfprintf(out_fh, fmt, ap);
	fprintf(out_fh, "\n");
	va_end(ap);
}

/*
 * out_get_percentage -- return percentage string
 */
const char *
out_get_percentage(double perc)
{
	static char str_buff[STR_MAX] = {0, };
	int ret = 0;

	if (perc > 0.0 && perc < 0.0001) {
		ret = util_snprintf(str_buff, STR_MAX, "%e %%", perc);
		if (ret < 0)
			return "";
	} else {
		int decimal = 0;
		if (perc >= 100.0 || perc < DBL_EPSILON)
			decimal = 0;
		else
			decimal = 6;

		ret = util_snprintf(str_buff, STR_MAX, "%.*f %%", decimal,
				perc);
		if (ret < 0)
			return "";
	}

	return str_buff;
}

/*
 * out_get_size_str -- return size string
 *
 * human - if 1 return size in human-readable format
 *         if 2 return size in bytes and human-readable format
 * otherwise return size in bytes.
 */
const char *
out_get_size_str(uint64_t size, int human)
{
	static char str_buff[STR_MAX] = {0, };
	char units[] = {
		'K', 'M', 'G', 'T', '\0'
	};
	const int nunits = sizeof(units) / sizeof(units[0]);
	int ret = 0;

	if (!human) {
		ret = util_snprintf(str_buff, STR_MAX, "%"PRIu64, size);
	} else {
		int i = -1;
		double dsize = (double)size;
		uint64_t csize = size;

		while (csize >= 1024 && i < nunits) {
			csize /= 1024;
			dsize /= 1024.0;
			i++;
		}

		if (i >= 0 && i < nunits)
			if (human == 1)
				ret = util_snprintf(str_buff, STR_MAX,
						"%.1f%c", dsize, units[i]);
			else
				ret = util_snprintf(str_buff, STR_MAX,
						"%.1f%c [%" PRIu64"]", dsize,
						units[i], size);
		else
			ret = util_snprintf(str_buff, STR_MAX, "%"PRIu64,
					size);
	}

	if (ret < 0)
		return "";

	return str_buff;
}

/*
 * out_get_uuid_str -- returns uuid in human readable format
 */
const char *
out_get_uuid_str(uuid_t uuid)
{
	static char uuid_str[UUID_STR_MAX] = {0, };

	int ret = util_uuid_to_string(uuid, uuid_str);
	if (ret != 0) {
		outv(2, "failed to covert uuid to string");
		return NULL;
	}
	return uuid_str;
}

/*
 * out_get_time_str -- returns time in human readable format
 */
const char *
out_get_time_str(time_t time)
{
	static char str_buff[STR_MAX] = {0, };
	struct tm tm;

	if (util_localtime(&time, &tm)) {
		strftime(str_buff, STR_MAX, TIME_STR_FMT, &tm);
	} else {
		int ret = util_snprintf(str_buff, STR_MAX, "unknown");
		if (ret < 0)
			return "";
	}

	return str_buff;
}

/*
 * out_get_ascii_str -- get string with printable ASCII dump buffer
 *
 * Convert non-printable ASCII characters to dot '.'
 * See: util_get_printable_ascii() function.
 */
static int
out_get_ascii_str(char *str, size_t str_len, const uint8_t *datap, size_t len)
{
	int c = 0;
	size_t i;
	char pch;

	if (str_len < len)
		return -1;

	for (i = 0; i < len; i++) {
		pch = util_get_printable_ascii((char)datap[i]);
		int t = util_snprintf(str + c, str_len - (size_t)c, "%c", pch);
		if (t < 0)
			return -1;
		c += t;
	}

	return c;
}

/*
 * out_get_hex_str -- get string with hexadecimal dump of buffer
 *
 * Hexadecimal bytes in format %02x, each one followed by space,
 * additional space after every 8th byte.
 */
static int
out_get_hex_str(char *str, size_t str_len, const uint8_t *datap, size_t len)
{
	int c = 0;
	size_t i;
	int t;

	if (str_len < (3 * len + 1))
		return -1;

	for (i = 0; i < len; i++) {
		/* add space after n*8 byte */
		if (i && (i % 8) == 0) {
			t = util_snprintf(str + c, str_len - (size_t)c, " ");
			if (t < 0)
				return -1;
			c += t;
		}
		t = util_snprintf(str + c, str_len - (size_t)c, "%02x ",
				datap[i]);
		if (t < 0)
			return -1;
		c += t;
	}

	return c;
}

/*
 * outv_hexdump -- print buffer in canonical hex+ASCII format
 *
 * Print offset in hexadecimal,
 * sixteen space-separated, two column, hexadecimal bytes,
 * followed by the same sixteen bytes converted to printable ASCII characters
 * enclosed in '|' characters.
 */
void
outv_hexdump(int vlevel, const void *addr, size_t len, size_t offset, int sep)
{
	if (!outv_check(vlevel) || len <= 0)
		return;

	const uint8_t *datap = (uint8_t *)addr;
	uint8_t row_hex_str[HEXDUMP_ROW_HEX_LEN] = {0, };
	uint8_t row_ascii_str[HEXDUMP_ROW_ASCII_LEN] = {0, };
	size_t curr = 0;
	size_t prev = 0;
	int repeated = 0;
	int n = 0;

	while (len) {
		size_t curr_len = min(len, HEXDUMP_ROW_WIDTH);

		/*
		 * Check if current row is the same as the previous one
		 * don't check it for first and last rows.
		 */
		if (len != curr_len && curr &&
				!memcmp(datap + prev, datap + curr, curr_len)) {
			if (!repeated) {
				/* print star only for the first repeated */
				fprintf(out_fh, "*\n");
				repeated = 1;
			}
		} else {
			repeated = 0;

			/* row with hexadecimal bytes */
			int rh = out_get_hex_str((char *)row_hex_str,
				HEXDUMP_ROW_HEX_LEN, datap + curr, curr_len);
			/* row with printable ascii chars */
			int ra = out_get_ascii_str((char *)row_ascii_str,
				HEXDUMP_ROW_ASCII_LEN, datap + curr, curr_len);

			if (ra && rh)
				n = fprintf(out_fh, "%08zx  %-*s|%-*s|\n",
					curr + offset,
					HEXDUMP_ROW_HEX_LEN, row_hex_str,
					HEXDUMP_ROW_WIDTH, row_ascii_str);
			prev = curr;
		}

		len -= curr_len;
		curr += curr_len;
	}

	if (sep && n) {
		while (--n)
			fprintf(out_fh, "%c", SEPARATOR_CHAR);
		fprintf(out_fh, "\n");
	}
}

/*
 * out_get_checksum -- return checksum string with result
 */
const char *
out_get_checksum(void *addr, size_t len, uint64_t *csump, size_t skip_off)
{
	static char str_buff[STR_MAX] = {0, };
	int ret = 0;

	uint64_t csum = util_checksum_compute(addr, len, csump, skip_off);

	if (*csump == htole64(csum))
		ret = util_snprintf(str_buff, STR_MAX, "0x%" PRIx64" [OK]",
			le64toh(csum));
	else
		ret = util_snprintf(str_buff, STR_MAX,
			"0x%" PRIx64 " [wrong! should be: 0x%" PRIx64 "]",
			le64toh(*csump), le64toh(csum));

	if (ret < 0)
		return "";

	return str_buff;
}

/*
 * out_get_btt_map_entry -- return BTT map entry with flags strings
 */
const char *
out_get_btt_map_entry(uint32_t map)
{
	static char str_buff[STR_MAX] = {0, };

	int is_init = (map & ~BTT_MAP_ENTRY_LBA_MASK) == 0;
	int is_zero = (map & ~BTT_MAP_ENTRY_LBA_MASK) ==
		BTT_MAP_ENTRY_ZERO;
	int is_error = (map & ~BTT_MAP_ENTRY_LBA_MASK) ==
		BTT_MAP_ENTRY_ERROR;
	int is_normal = (map & ~BTT_MAP_ENTRY_LBA_MASK) ==
		BTT_MAP_ENTRY_NORMAL;

	uint32_t lba = map & BTT_MAP_ENTRY_LBA_MASK;

	int ret = util_snprintf(str_buff, STR_MAX, "0x%08x state: %s", lba,
			is_init ? "init" :
			is_zero ? "zero" :
			is_error ? "error" :
			is_normal ? "normal" : "unknown");

	if (ret < 0)
		return "";

	return str_buff;
}

/*
 * out_get_pool_type_str -- get pool type string
 */
const char *
out_get_pool_type_str(pmem_pool_type_t type)
{
	switch (type) {
	case PMEM_POOL_TYPE_LOG:
		return "log";
	case PMEM_POOL_TYPE_BLK:
		return "blk";
	case PMEM_POOL_TYPE_OBJ:
		return "obj";
	case PMEM_POOL_TYPE_BTT:
		return "btt";
	default:
		return "unknown";
	}
}

/*
 * out_get_pool_signature -- return signature of specified pool type
 */
const char *
out_get_pool_signature(pmem_pool_type_t type)
{
	switch (type) {
	case PMEM_POOL_TYPE_LOG:
		return LOG_HDR_SIG;
	case PMEM_POOL_TYPE_BLK:
		return BLK_HDR_SIG;
	case PMEM_POOL_TYPE_OBJ:
		return OBJ_HDR_SIG;
	default:
		return NULL;
	}
}

/*
 * out_get_chunk_type_str -- get chunk type string
 */
const char *
out_get_chunk_type_str(enum chunk_type type)
{
	switch (type) {
	case CHUNK_TYPE_FOOTER:
		return "footer";
	case CHUNK_TYPE_FREE:
		return "free";
	case CHUNK_TYPE_USED:
		return "used";
	case CHUNK_TYPE_RUN:
		return "run";
	case CHUNK_TYPE_UNKNOWN:
	default:
		return "unknown";
	}
}

/*
 * out_get_chunk_flags -- get names of set flags for chunk header
 */
const char *
out_get_chunk_flags(uint16_t flags)
{
	if (flags & CHUNK_FLAG_COMPACT_HEADER)
		return "compact header";
	else if (flags & CHUNK_FLAG_HEADER_NONE)
		return "header none";

	return "";
}

/*
 * out_get_zone_magic_str -- get zone magic string with additional
 * information about correctness of the magic value
 */
const char *
out_get_zone_magic_str(uint32_t magic)
{
	static char str_buff[STR_MAX] = {0, };

	const char *correct = NULL;
	switch (magic) {
	case 0:
		correct = "uninitialized";
		break;
	case ZONE_HEADER_MAGIC:
		correct = "OK";
		break;
	default:
		correct = "wrong! should be " STR(ZONE_HEADER_MAGIC);
		break;
	}

	int ret = util_snprintf(str_buff, STR_MAX, "0x%08x [%s]", magic,
			correct);

	if (ret < 0)
		return "";

	return str_buff;
}

/*
 * out_get_pmemoid_str -- get PMEMoid string
 */
const char *
out_get_pmemoid_str(PMEMoid oid, uint64_t uuid_lo)
{
	static char str_buff[STR_MAX] = {0, };
	int free_cor = 0;
	int ret = 0;
	char *correct = "OK";
	if (oid.pool_uuid_lo && oid.pool_uuid_lo != uuid_lo) {
		ret = util_snprintf(str_buff, STR_MAX,
			"wrong! should be 0x%016"PRIx64, uuid_lo);
		if (ret < 0)
			err(1, "snprintf: %d", ret);
		correct = strdup(str_buff);
		if (!correct)
			err(1, "Cannot allocate memory for PMEMoid string\n");
		free_cor = 1;
	}

	ret = util_snprintf(str_buff, STR_MAX,
			"off: 0x%016"PRIx64" pool_uuid_lo: 0x%016"
			PRIx64" [%s]", oid.off, oid.pool_uuid_lo, correct);

	if (free_cor)
		free(correct);

	if (ret < 0)
		err(1, "snprintf: %d", ret);

	return str_buff;
}

/*
 * out_get_arch_machine_class_str -- get a string representation of the machine
 * class
 */
const char *
out_get_arch_machine_class_str(uint8_t machine_class)
{

	switch (machine_class) {
	case PMDK_MACHINE_CLASS_64:
		return "64";
	default:
		return "unknown";
	}
}

/*
 * out_get_arch_data_str -- get a string representation of the data endianness
 */
const char *
out_get_arch_data_str(uint8_t data)
{
	switch (data) {
	case PMDK_DATA_LE:
		return "2's complement, little endian";
	case PMDK_DATA_BE:
		return "2's complement, big endian";
	default:
		return "unknown";
	}
}

/*
 * out_get_arch_machine_str -- get a string representation of the machine type
 */
const char *
out_get_arch_machine_str(uint16_t machine)
{
	static char str_buff[STR_MAX] = {0, };
	switch (machine) {
	case PMDK_MACHINE_X86_64:
		return "AMD X86-64";
	case PMDK_MACHINE_AARCH64:
		return "Aarch64";
	case PMDK_MACHINE_PPC64:
		return "PPC64";
	default:
		break;
	}

	int ret = util_snprintf(str_buff, STR_MAX, "unknown %u", machine);
	if (ret < 0)
		return "unknown";
	return str_buff;
}

/*
 * out_get_last_shutdown_str -- get a string representation of the finish state
 */
const char *
out_get_last_shutdown_str(uint8_t dirty)
{
	if (dirty)
		return "dirty";
	else
		return "clean";
}

/*
 * out_get_alignment_descr_str -- get alignment descriptor string
 */
const char *
out_get_alignment_desc_str(uint64_t ad, uint64_t valid_ad)
{
	static char str_buff[STR_MAX] = {0, };
	int ret = 0;

	if (ad == valid_ad)
		ret = util_snprintf(str_buff, STR_MAX, "0x%016"PRIx64"[OK]",
				ad);
	else
		ret = util_snprintf(str_buff, STR_MAX, "0x%016"PRIx64" "
			"[wrong! should be 0x%016"PRIx64"]", ad, valid_ad);

	if (ret < 0)
		return "";

	return str_buff;
}

/*
 * out_concat -- concatenate the new element to the list of strings
 *
 * If concatenation is successful it increments current position in the output
 * string and number of elements in the list. Elements are separated with ", ".
 */
static int
out_concat(char *str_buff, int *curr, int *count, const char *str)
{
	ASSERTne(str_buff, NULL);
	ASSERTne(curr, NULL);
	ASSERTne(str, NULL);

	const char *separator = (count != NULL && *count > 0) ? ", " : "";
	int ret = util_snprintf(str_buff + *curr,
		(size_t)(STR_MAX - *curr), "%s%s", separator, str);
	if (ret < 0)
		return -1;
	*curr += ret;
	if (count)
		++(*count);
	return 0;
}

/*
 * out_get_incompat_features_str -- (internal) get a string with names of
 *                                  incompatibility flags
 */
const char *
out_get_incompat_features_str(uint32_t incompat)
{
	static char str_buff[STR_MAX] = {0};
	features_t features = {POOL_FEAT_ZERO, incompat, POOL_FEAT_ZERO};
	int ret = 0;

	if (incompat == 0) {
		/* print the value only */
		return "0x0";
	} else {
		/* print the value and the left square bracket */
		ret = util_snprintf(str_buff, STR_MAX, "0x%x [", incompat);
		if (ret < 0) {
			ERR("snprintf for incompat features: %d", ret);
			return "<error>";
		}

		/* print names of known options */
		int count = 0;
		int curr = ret;
		features_t found;
		const char *feat;

		while (((feat = util_feature2str(features, &found))) != NULL) {
			util_feature_disable(&features, found);
			ret = out_concat(str_buff, &curr, &count, feat);
			if (ret < 0)
				return "";
		}

		/* check if any unknown flags are set */
		if (!util_feature_is_zero(features)) {
			if (out_concat(str_buff, &curr, &count,
					"?UNKNOWN_FLAG?"))
				return "";
		}

		/* print the right square bracket */
		if (out_concat(str_buff, &curr, NULL, "]"))
			return "";
	}
	return str_buff;
}
