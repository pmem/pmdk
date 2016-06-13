/*
 * Copyright 2014-2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * util.h -- internal definitions for util module
 */

extern unsigned long long Pagesize;

extern int Mmap_no_random;
extern void *Mmap_hint;

/*
 * overridable names for malloc & friends used by this library
 */
typedef void *(*Malloc_func)(size_t size);
typedef void (*Free_func)(void *ptr);
typedef void *(*Realloc_func)(void *ptr, size_t size);
typedef char *(*Strdup_func)(const char *s);

extern Malloc_func Malloc;
extern Free_func Free;
extern Realloc_func Realloc;
extern Strdup_func Strdup;
extern void *Zalloc(size_t sz);

void util_set_alloc_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s));
void *util_map(int fd, size_t len, int cow, size_t req_align);
int util_unmap(void *addr, size_t len);

int util_tmpfile(const char *dir, const char *templ);
void *util_map_tmpfile(const char *dir, size_t size, size_t req_align);

/*
 * Number of bits per type in alignment descriptor
 */
#define ALIGNMENT_DESC_BITS		4

/*
 * Macro calculates number of elements in given table
 */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))
#endif

/*
 * architecture identification flags
 *
 * These flags allow to unambiguously determine the architecture
 * on which the pool was created.
 *
 * The alignment_desc field contains information about alignment
 * of the following basic types:
 * - char
 * - short
 * - int
 * - long
 * - long long
 * - size_t
 * - off_t
 * - float
 * - double
 * - long double
 * - void *
 *
 * The alignment of each type is computer as an offset of field
 * of specific type in the following structure:
 * struct {
 *	char byte;
 *	type field;
 * };
 *
 * The value is decremented by 1 and masked by 4 bits.
 * Multiple alignment are stored on consecutive 4 bits of each
 * type in order specified above.
 */
struct arch_flags {
	uint64_t alignment_desc;	/* alignment descriptor */
	uint8_t ei_class;		/* ELF format file class */
	uint8_t ei_data;		/* ELF format data encoding */
	uint8_t reserved[4];
	uint16_t e_machine;		/* required architecture */
};

/*
 * header used at the beginning of all types of memory pools
 *
 * for pools build on persistent memory, the integer types
 * below are stored in little-endian byte order.
 */
#define POOL_HDR_SIG_LEN 8
#define POOL_HDR_UUID_LEN	16 /* uuid byte length */
#define POOL_HDR_UUID_STR_LEN	37 /* uuid string length */
#define POOL_HDR_UUID_GEN_FILE	"/proc/sys/kernel/random/uuid"

typedef unsigned char uuid_t[POOL_HDR_UUID_LEN]; /* 16 byte binary uuid value */

struct pool_hdr {
	char signature[POOL_HDR_SIG_LEN];
	uint32_t major;			/* format major version number */
	uint32_t compat_features;	/* mask: compatible "may" features */
	uint32_t incompat_features;	/* mask: "must support" features */
	uint32_t ro_compat_features;	/* mask: force RO if unsupported */
	uuid_t poolset_uuid; /* pool set UUID */
	uuid_t uuid; /* UUID of this file */
	uuid_t prev_part_uuid; /* prev part */
	uuid_t next_part_uuid; /* next part */
	uuid_t prev_repl_uuid; /* prev replica */
	uuid_t next_repl_uuid; /* next replica */
	uint64_t crtime;		/* when created (seconds since epoch) */
	struct arch_flags arch_flags;	/* architecture identification flags */
	unsigned char unused[3944];	/* must be zero */
	uint64_t checksum;		/* checksum of above fields */
};

#define POOL_HDR_SIZE	(sizeof(struct pool_hdr))

void util_convert2le_hdr(struct pool_hdr *hdrp);
void util_convert2h_hdr_nocheck(struct pool_hdr *hdrp);
int util_checksum(void *addr, size_t len, uint64_t *csump, int insert);
int util_convert_hdr(struct pool_hdr *hdrp);
int util_get_arch_flags(struct arch_flags *arch_flags);
int util_check_arch_flags(const struct arch_flags *arch_flags);
int util_is_absolute_path(const char *path);

/*
 * macros for micromanaging range protections for the debug version
 */
#ifdef DEBUG

#define RANGE_RO(addr, len) ASSERT(util_range_ro(addr, len) >= 0)
#define RANGE_RW(addr, len) ASSERT(util_range_rw(addr, len) >= 0)

#else

/* nondebug version */
#define RANGE_RO(addr, len)
#define RANGE_RW(addr, len)

#endif	/* DEBUG */

/*
 * pool sets & replicas
 */
#define POOLSET_HDR_SIG "PMEMPOOLSET"
#define POOLSET_HDR_SIG_LEN 11	/* does NOT include '\0' */

#define POOLSET_REPLICA_SIG "REPLICA"
#define POOLSET_REPLICA_SIG_LEN 7	/* does NOT include '\0' */

struct pool_set_part {
	/* populated by a pool set file parser */
	const char *path;
	size_t filesize;	/* aligned to page size */
	int fd;
	int created;		/* indicates newly created (zeroed) file */

	/* util_poolset_open/create */
	void *hdr;		/* base address of header */
	size_t hdrsize;		/* size of the header mapping */
	void *addr;		/* base address of the mapping */
	size_t size;		/* size of the mapping - page aligned */
	int rdonly;
	uuid_t uuid;
};

struct remote_replica {
	char *node_addr;	/* address of a remote node */
	char *pool_desc;	/* descriptor of a poolset */
};

struct pool_replica {
	unsigned nparts;
	size_t repsize;		/* total size of all the parts (mappings) */
	int is_pmem;		/* true if all the parts are in PMEM */
	struct remote_replica *remote;	/* not NULL if the replica */
					/* is a remote one */
	struct pool_set_part part[];
};

struct pool_set {
	unsigned nreplicas;
	uuid_t uuid;
	int rdonly;
	int zeroed;		/* true if all the parts are new files */
	size_t poolsize;	/* the smallest replica size */
	int remote;		/* true if contains a remote replica */
	struct pool_replica *replica[];
};

#define REP(set, r)\
	((set)->replica[((set)->nreplicas + (r)) % (set)->nreplicas])

#define PART(rep, p)\
	((rep)->part[((rep)->nparts + (p)) % (rep)->nparts])

#define HDR(rep, p)\
	((struct pool_hdr *)(PART(rep, p).hdr))

/*
 * Structure for binary version of uuid. From RFC4122,
 * https://tools.ietf.org/html/rfc4122
 */
struct uuid {
	uint32_t time_low;
	uint16_t time_mid;
	uint16_t time_hi_and_ver;
	uint8_t clock_seq_hi;
	uint8_t	clock_seq_low;
	uint8_t node[6];
};

void util_init(void);

int util_range_ro(void *addr, size_t len);
int util_range_rw(void *addr, size_t len);
int util_range_none(void *addr, size_t len);

int util_is_zeroed(const void *addr, size_t len);

int util_feature_check(struct pool_hdr *hdrp, uint32_t incompat,
				uint32_t ro_compat, uint32_t compat);

char *util_map_hint_unused(void *addr, size_t len, size_t align);
char *util_map_hint(size_t len, size_t req_align);

int util_poolset_parse(const char *path, int fd, struct pool_set **setp);
int util_poolset_read(struct pool_set **setp, const char *path);
void util_poolset_close(struct pool_set *set, int del);
void util_poolset_free(struct pool_set *set);
int util_poolset_chmod(struct pool_set *set, mode_t mode);
void util_poolset_fdclose(struct pool_set *set);
int util_is_poolset(const char *path);
int util_poolset_foreach_part(const char *path,
	int (*cb)(const char *part_file, void *arg), void *arg);
size_t util_poolset_size(const char *path);
int util_file_create(const char *path, size_t size, size_t minsize);
int util_file_open(const char *path, size_t *size, size_t minsize, int flags);
int util_uuid_to_string(uuid_t u, char *buf);
int util_uuid_from_string(const char uuid[POOL_HDR_UUID_STR_LEN],
	struct uuid *ud);
int util_uuid_generate(uuid_t uuid);

int util_pool_create(struct pool_set **setp, const char *path, size_t poolsize,
	size_t minsize, const char *sig,
	uint32_t major, uint32_t compat, uint32_t incompat, uint32_t ro_compat);
int util_pool_create_uuids(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, const char *sig,
	uint32_t major, uint32_t compat, uint32_t incompat, uint32_t ro_compat,
	const unsigned char *poolset_uuid, const unsigned char *first_part_uuid,
	const unsigned char *prev_repl_uuid,
	const unsigned char *next_repl_uuid,
	const unsigned char *arch_flags);

int util_map_hdr(struct pool_set_part *part, int flags);
int util_unmap_hdr(struct pool_set_part *part);

int util_pool_open_nocheck(struct pool_set **setp, const char *path,
		int rdonly);
int util_pool_open(struct pool_set **setp, const char *path, int rdonly,
	size_t minsize, const char *sig,
	uint32_t major, uint32_t compat, uint32_t incompat, uint32_t ro_compat);
int util_pool_open_remote(struct pool_set **setp, const char *path, int rdonly,
	size_t minsize, char *sig, uint32_t *major,
	uint32_t *compat, uint32_t *incompat, uint32_t *ro_compat,
	unsigned char *poolset_uuid, unsigned char *first_part_uuid,
	unsigned char *prev_repl_uuid, unsigned char *next_repl_uuid,
	unsigned char *arch_flags);

int util_parse_size(const char *str, size_t *sizep);

/*
 * util_setbit -- setbit macro substitution which properly deals with types
 */
static inline void
util_setbit(uint8_t *b, uint32_t i)
{
	b[i / 8] = (uint8_t)(b[i / 8] | (uint8_t)(1 << (i % 8)));
}

/*
 * util_clrbit -- clrbit macro substitution which properly deals with types
 */
static inline void
util_clrbit(uint8_t *b, uint32_t i)
{
	b[i / 8] = (uint8_t)(b[i / 8] & (uint8_t)(~(1 << (i % 8))));
}

#define util_isset(a, i) isset(a, i)
#define util_isclr(a, i) isclr(a, i)

#define util_flag_isset(a, f) ((a) & (f))
#define util_flag_isclr(a, f) (((a) & (f)) == 0)

#if !defined(likely)
#if defined(__GNUC__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (!!(x))
#define unlikely(x) (!!(x))
#endif
#endif

/*
 * set of macros for determining the alignment descriptor
 */
#define DESC_MASK		((1 << ALIGNMENT_DESC_BITS) - 1)
#define alignment_of(t)		offsetof(struct { char c; t x; }, x)
#define alignment_desc_of(t)	(((uint64_t)alignment_of(t) - 1) & DESC_MASK)
#define alignment_desc()\
(alignment_desc_of(char)	<<  0 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(short)	<<  1 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(int)		<<  2 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long)	<<  3 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long long)	<<  4 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(size_t)	<<  5 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(off_t)	<<  6 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(float)	<<  7 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(double)	<<  8 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(long double)	<<  9 * ALIGNMENT_DESC_BITS) |\
(alignment_desc_of(void *)	<< 10 * ALIGNMENT_DESC_BITS)

#ifndef _WIN32
typedef struct stat util_stat_t;
#define util_fstat	fstat
#define util_stat	stat
#define util_lseek	lseek
#else
typedef struct _stat64 util_stat_t;
#define util_fstat	_fstat64
#define util_stat	_stat64
#define util_lseek	_lseeki64
#endif

#ifndef _MSC_VER
#define COMPILE_ERROR_ON(cond) ((void)sizeof(char[(cond) ? -1 : 1]))
#define ASSERT_COMPILE_ERROR_ON(cond) COMPILE_ERROR_ON(cond)
#else
#define COMPILE_ERROR_ON(cond) C_ASSERT(!(cond))
/* XXX - can't be done with C_ASSERT() unless we have __builtin_constant_p() */
#define ASSERT_COMPILE_ERROR_ON(cond)
#endif

#ifndef _MSC_VER
#define ATTR_CONSTRUCTOR __attribute__((constructor)) static
#define ATTR_DESTRUCTOR __attribute__((destructor)) static
#else
#define ATTR_CONSTRUCTOR
#define ATTR_DESTRUCTOR
#endif
