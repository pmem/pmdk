/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2022, Intel Corporation */

/*
 * btt_layout.h -- block translation table on-media layout definitions
 */

/*
 * Layout of BTT info block.  All integers are stored little-endian.
 */

#ifndef BTT_LAYOUT_H
#define BTT_LAYOUT_H 1

#ifdef __cplusplus
extern "C" {
#endif

#define BTT_ALIGNMENT ((uintptr_t)4096)	/* alignment of all BTT structures */
#define BTTINFO_SIG_LEN 16
#define BTTINFO_UUID_LEN 16
#define BTTINFO_UNUSED_LEN 3968
#define BTTINFO_SIG		"BTT_ARENA_INFO\0"

struct btt_info {
	char sig[BTTINFO_SIG_LEN];	/* must be "BTT_ARENA_INFO\0\0" */
	uint8_t uuid[BTTINFO_UUID_LEN];	/* BTT UUID */
	uint8_t parent_uuid[BTTINFO_UUID_LEN];	/* UUID of container */
	uint32_t flags;			/* see flag bits below */
	uint16_t major;			/* major version */
	uint16_t minor;			/* minor version */
	uint32_t external_lbasize;	/* advertised LBA size (bytes) */
	uint32_t external_nlba;		/* advertised LBAs in this arena */
	uint32_t internal_lbasize;	/* size of data area blocks (bytes) */
	uint32_t internal_nlba;		/* number of blocks in data area */
	uint32_t nfree;			/* number of free blocks */
	uint32_t infosize;		/* size of this info block */

	/*
	 * The following offsets are relative to the beginning of
	 * the btt_info block.
	 */
	uint64_t nextoff;		/* offset to next arena (or zero) */
	uint64_t dataoff;		/* offset to arena data area */
	uint64_t mapoff;		/* offset to area map */
	uint64_t flogoff;		/* offset to area flog */
	uint64_t infooff;		/* offset to backup info block */

	char unused[BTTINFO_UNUSED_LEN];	/* must be zero */

	uint64_t checksum;		/* Fletcher64 of all fields */
};

/*
 * Definitions for flags mask for btt_info structure above.
 */
#define BTTINFO_FLAG_ERROR	0x00000001 /* error state (read-only) */
#define BTTINFO_FLAG_ERROR_MASK	0x00000001 /* all error bits */

/*
 * Current on-media format versions.
 */
/*
 * #define BTTINFO_MAJOR_VERSION 1
 */
#define BTTINFO_MAJOR_VERSION 2
#define BTTINFO_MINOR_VERSION 1

/*
 * Layout of a BTT "flog" entry.  All integers are stored little-endian.
 *
 * The "nfree" field in the BTT info block determines how many of these
 * flog entries there are, and each entry consists of two of the following
 * structs (entry updates alternate between the two structs), padded up
 * to a cache line boundary to isolate adjacent updates.
 */

#define BTT_FLOG_PAIR_ALIGN ((uintptr_t)64)

struct btt_flog {
	uint32_t lba;		/* last pre-map LBA using this entry */
	uint32_t old_map;	/* old post-map LBA (the freed block) */
	uint32_t new_map;	/* new post-map LBA */
	uint32_t seq;		/* sequence number (01, 10, 11) */
};

/*
 * Layout of a BTT "map" entry.  4-byte internal LBA offset, little-endian.
 */
#define BTT_MAP_ENTRY_SIZE 4
#define BTT_MAP_ENTRY_ERROR 0x40000000U
#define BTT_MAP_ENTRY_ZERO 0x80000000U
#define BTT_MAP_ENTRY_NORMAL 0xC0000000U
#define BTT_MAP_ENTRY_LBA_MASK 0x3fffffffU
#define BTT_MAP_LOCK_ALIGN ((uintptr_t)64)

/*
 * BTT layout properties...
 */
#define BTT_MIN_SIZE ((1u << 20) * 16)
#define BTT_MAX_ARENA (1ull << 39) /* 512GB per arena */
#define BTT_MIN_LBA_SIZE (size_t)512
#define BTT_INTERNAL_LBA_ALIGNMENT 256U

#define BTT_DEFAULT_NFREE 256

#ifdef __cplusplus
}
#endif

#endif
