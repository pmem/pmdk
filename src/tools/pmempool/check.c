/*
 * Copyright (c) 2014-2015, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * check.c -- pmempool check command source file
 */
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>
#define	__USE_UNIX98
#include <unistd.h>
#include "common.h"
#include "output.h"
#include "libpmemblk.h"
#include "libpmemlog.h"
#include "btt.h"

/*
 * arena -- internal structure for holding BTT Info and offset
 */
struct arena {
	TAILQ_ENTRY(arena) next;
	struct btt_info btt_info; /* BTT Info header */
	uint32_t id;		/* arena id */
	bool valid;		/* BTT Info header checksum is valid */
	off_t offset;		/* offset in file */
	uint8_t *flog;		/* flog entries */
	size_t flogsize;	/* flog area size */
	uint32_t *map;		/* map entries */
	size_t mapsize;		/* map area size */
};

/*
 * pmempool_check -- context and arguments for check command
 */
struct pmempool_check {
	int verbose;		/* verbosity level */
	char *fname;		/* file name */
	int fd;			/* file descriptor */
	bool repair;		/* do repair */
	bool backup;		/* do backup */
	char *backup_fname;	/* backup file name */
	bool exec;		/* do execute */
	pmem_pool_type_t ptype; /* pool type */
	union {
		struct pool_hdr pool;
		struct pmemlog log;
		struct pmemblk blk;
	} hdr;			/* headers */
	enum {
		UUID_NOP = 0,	/* nothing changed */
		UUID_FROM_BTT,	/* UUID restored from valid BTT Info header */
		UUID_REGENERATED, /* UUID regenerated */
	} uuid_op;		/* operation on UUID */
	struct arena bttc;	/* arena cache */
	TAILQ_HEAD(arenashead, arena) arenas;
	uint32_t narenas;	/* number of arenas */
	char ans;		/* default answer on all questions or '?' */
};

typedef enum
{
	CHECK_RESULT_CONSISTENT,
	CHECK_RESULT_CONSISTENT_BREAK,
	CHECK_RESULT_NOT_CONSISTENT,
	CHECK_RESULT_REPAIRED,
	CHECK_RESULT_CANNOT_REPAIR,
	CHECK_RESULT_ERROR
} check_result_t;

/*
 * pmempool_check_step -- structure for check and repair steps
 */
struct pmempool_check_step {
	check_result_t (*func)(struct pmempool_check *); /* step function */
	pmem_pool_type_t type;	/* allowed pool types */
};

#define	BACKUP_SUFFIX	".bak"
#define	BTT_INFO_SIG	"BTT_ARENA_INFO\0"

/*
 * btt_context -- context for using btt API
 */
struct btt_context {
	struct pmempool_check *pcp;	/* pmempool check context */
	void *addr;			/* starting address */
	uint64_t len;			/* area size */
};

/*
 * pmempool_check_nsread -- btt callback for reading
 */
static int
pmempool_check_nsread(void *ns, int lane, void *buf, size_t count,
		off_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + count > nsc->len) {
		errno = EINVAL;
		return -1;
	}

	memcpy(buf, nsc->addr + off, count);

	return 0;
}

/*
 * pmempool_check_nswrite -- btt callback for writing
 */
static int
pmempool_check_nswrite(void *ns, int lane, const void *buf, size_t count,
		off_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + count > nsc->len) {
		errno = EINVAL;
		return -1;
	}

	memcpy(nsc->addr + off, buf, count);

	return 0;
}

/*
 * pmempool_check_nsmap -- btt callback for memory mapping
 */
static ssize_t
pmempool_check_nsmap(void *ns, int lane, void **addrp, size_t len,
		off_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + len >= nsc->len) {
		errno = EINVAL;
		return -1;
	}

	/*
	 * Since the entire file is memory-mapped, this callback
	 * can always provide the entire length requested.
	 */
	*addrp = nsc->addr + off;

	return len;
}

/*
 * pmempool_check_nssync -- btt callback for memory synchronization
 */
static void
pmempool_check_nssync(void *ns, int lane, void *addr, size_t len)
{
	/* do nothing */
}

/*
 * pmempool_check_nszero -- btt callback for zeroing memory
 */
static int
pmempool_check_nszero(void *ns, int lane, size_t len, off_t off)
{
	struct btt_context *nsc = (struct btt_context *)ns;

	if (off + len >= nsc->len) {
		errno = EINVAL;
		return -1;
	}
	memset(nsc->addr + off, 0, len);

	return 0;
}

/*
 * list_item -- item for simple list
 */
struct list_item {
	LIST_ENTRY(list_item) next;
	uint32_t val;
};

/*
 * list -- simple list for storing numbers
 */
struct list {
	LIST_HEAD(listhead, list_item) head;
	uint32_t count;
};

/*
 * list_alloc -- allocate an empty list
 */
static struct list *
list_alloc(void)
{
	struct list *list = malloc(sizeof (struct list));
	if (!list)
		err(1, "Cannot allocate memory for list");
	LIST_INIT(&list->head);
	list->count = 0;
	return list;
}

/*
 * list_push -- insert new element to the list
 */
static void
list_push(struct list *list, uint32_t val)
{
	struct list_item *item = malloc(sizeof (*item));
	if (!item)
		err(1, "Cannot allocate memory for list item");
	item->val = val;
	list->count++;
	LIST_INSERT_HEAD(&list->head, item, next);
}

/*
 * list_pop -- pop element from list head
 */
static int
list_pop(struct list *list, uint32_t *valp)
{
	if (!LIST_EMPTY(&list->head)) {
		struct list_item *i = LIST_FIRST(&list->head);
		LIST_REMOVE(i, next);
		if (valp)
			*valp = i->val;
		free(i);

		list->count--;

		return 1;
	}
	return 0;
}

/*
 * list_free -- free the list
 */
static void
list_free(struct list *list)
{
	while (list_pop(list, NULL));
	free(list);
}

/*
 * pmempool_check_btt_ns_callback -- callbacks for btt API
 */
struct ns_callback pmempool_check_btt_ns_callback = {
	.nsread = pmempool_check_nsread,
	.nswrite = pmempool_check_nswrite,
	.nsmap = pmempool_check_nsmap,
	.nssync = pmempool_check_nssync,
	.nszero = pmempool_check_nszero,
};

/*
 * pmempool_check_default -- default arguments for check command
 */
const struct pmempool_check pmempool_check_default = {
	.verbose	= 1,
	.fname		= NULL,
	.fd		= -1,
	.repair		= false,
	.backup		= false,
	.backup_fname	= NULL,
	.exec		= true,
	.ptype		= PMEM_POOL_TYPE_UNKNOWN,
	.narenas	= 0,
	.ans		= '?',
};

/*
 * help_str -- string for help message
 */
static const char *help_str =
"Check consistency of a pool\n"
"\n"
"Common options:\n"
"  -r, --repair         try to repair a pool file if possible\n"
"  -y, --yes            answer yes to all questions\n"
"  -N, --no-exec        don't execute, just show what would be done\n"
"  -b, --backup <file>  create backup of a pool file before executing\n"
"  -q, --quiet          be quiet and don't print any messages\n"
"  -v, --verbose        increase verbosity level\n"
"  -h, --help           display this help and exit\n"
"\n"
"For complete documentation see %s-check(1) manual page.\n"
;

/*
 * long_options -- command line options
 */
static const struct option long_options[] = {
	{"repair",	no_argument,		0,	'r'},
	{"yes",		no_argument,		0,	'y'},
	{"no-exec",	no_argument,		0,	'N'},
	{"backup",	required_argument,	0,	'b'},
	{"quiet",	no_argument,		0,	'q'},
	{"verbose",	no_argument,		0,	'v'},
	{"help",	no_argument,		0,	'h'},
	{0,		0,			0,	 0 },
};

/*
 * print_usage -- print short description of application's usage
 */
static void
print_usage(char *appname)
{
	printf("Usage: %s check [<args>] <file>\n", appname);
}

/*
 * print_version -- print version string
 */
static void
print_version(char *appname)
{
	printf("%s %s\n", appname, SRCVERSION);
}

/*
 * pmempool_check_help -- print help message for check command
 */
void
pmempool_check_help(char *appname)
{
	print_usage(appname);
	print_version(appname);
	printf(help_str, appname);
}

/*
 * pmempool_check_parse_args -- parse command line arguments
 */
static int
pmempool_check_parse_args(struct pmempool_check *pcp, char *appname,
		int argc, char *argv[])
{
	int opt;
	while ((opt = getopt_long(argc, argv, "hvrNb:qy",
			long_options, NULL)) != -1) {
		switch (opt) {
		case 'r':
			pcp->repair = true;
			break;
		case 'y':
			pcp->ans = 'y';
			break;
		case 'N':
			pcp->exec = false;
			break;
		case 'b':
			pcp->backup = true;
			pcp->backup_fname = optarg;
			break;
		case 'q':
			pcp->verbose = 0;
			break;
		case 'v':
			pcp->verbose = 2;
			break;
		case 'h':
			pmempool_check_help(appname);
			exit(EXIT_SUCCESS);
		default:
			print_usage(appname);
			exit(EXIT_FAILURE);
		}
	}

	if (optind < argc) {
		pcp->fname = argv[optind];
	} else {
		print_usage(appname);
		exit(EXIT_FAILURE);
	}

	if (!pcp->repair && !pcp->exec) {
		out_err("'-N' option requires '-r'\n");
		exit(EXIT_FAILURE);
	}

	if (!pcp->repair && pcp->backup) {
		out_err("'-b' option requires '-r'\n");
		exit(EXIT_FAILURE);
	}

	return 0;
}

/*
 * pmempool_check_cp -- copy file from fsrc to fdst
 */
static int
pmempool_check_cp(char *fsrc, char *fdst)
{
	int sfd;
	int dfd;

	if ((sfd = open(fsrc, O_RDONLY)) < 0)
		err(1, "%s", fsrc);

	struct stat buff;
	if (fstat(sfd, &buff)) {
		warn("%s", fsrc);
		close(sfd);
		return -1;
	}

	if ((dfd = open(fdst, O_WRONLY|O_CREAT, buff.st_mode)) < 0) {
		warn("%s", fdst);
		close(sfd);
		return -1;
	}

	ssize_t ret = sendfile(dfd, sfd, NULL, buff.st_size);
	if (ret == -1)
		err(1, "copying file '%s' to '%s' failed", fsrc, fdst);

	close(sfd);
	close(dfd);

	return !(ret == buff.st_size);
}

/*
 * pmempool_check_create_backup -- create backup file
 */
static int
pmempool_check_create_backup(struct pmempool_check *pcp)
{
	outv(1, "creating backup file: %s\n", pcp->backup_fname);
	return pmempool_check_cp(pcp->fname, pcp->backup_fname);
}

/*
 * pmempool_check_get_first_valid_btt -- return offset to first valid BTT Info
 *
 * - Return offset to first valid BTT Info header in pool file.
 * - Start at specific offset.
 * - Convert BTT Info header to host endianness.
 * - Return the BTT Info header by pointer.
 */
static off_t
pmempool_check_get_first_valid_btt(struct pmempool_check *pcp, struct btt_info
		*infop, off_t offset)
{
	/*
	 * Starting at offset, read every page and check for
	 * valid BTT Info Header. Check signature and checksum.
	 */
	while (pread(pcp->fd, infop, sizeof (*infop), offset)
			== sizeof (*infop)) {
		if (memcmp(infop->sig, BTT_INFO_SIG,
					BTTINFO_SIG_LEN) == 0 &&
			util_checksum(infop, sizeof (*infop),
				&infop->checksum, 0)) {

			util_convert2h_btt_info(infop);
			return offset;
		}

		offset += BTT_ALIGNMENT;
	}

	return 0;
}

/*
 * pmempool_check_get_first_valid_arena -- get first valid BTT Info in arena
 */
static int
pmempool_check_get_first_valid_arena(struct pmempool_check *pcp,
		struct arena *arenap)
{
	off_t offset = pmempool_check_get_first_valid_btt(pcp,
			&arenap->btt_info, 2 * BTT_ALIGNMENT);

	if (offset) {
		arenap->valid = true;
		arenap->offset = offset;
		return 1;
	}

	return 0;
}

/*
 * pmempool_check_clear_arenas -- clear list of arenas
 */
static void
pmempool_check_clear_arenas(struct pmempool_check *pcp)
{
	while (!TAILQ_EMPTY(&pcp->arenas)) {
		struct arena *arenap = TAILQ_FIRST(&pcp->arenas);
		if (arenap->map)
			free(arenap->map);
		if (arenap->flog)
			free(arenap->flog);
		TAILQ_REMOVE(&pcp->arenas, arenap, next);
		free(arenap);
	}
}

/*
 * pmempool_check_insert_arena -- insert arena to list
 */
static void
pmempool_check_insert_arena(struct pmempool_check *pcp, struct arena
		*arenap)
{
	TAILQ_INSERT_TAIL(&pcp->arenas, arenap, next);
	pcp->narenas++;
}

/*
 * pmempool_check_pool_hdr -- try to repair pool_hdr
 */
static check_result_t
pmempool_check_pool_hdr(struct pmempool_check *pcp)
{
	outv(2, "checking pool header\n");
	if (pread(pcp->fd, &pcp->hdr.pool, sizeof (pcp->hdr.pool), 0) !=
			sizeof (pcp->hdr.pool)) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("cannot read pool_hdr\n");
		return CHECK_RESULT_ERROR;
	}

	if (util_check_memory((const uint8_t *)&pcp->hdr.pool,
			sizeof (pcp->hdr.pool), 0) != 0) {
		/*
		 * Validate checksum and return immediately if checksum is
		 * correct.
		 * If checksum is invalid and the repair flag is not set
		 * print message and return error.
		 */
		if (util_checksum(&pcp->hdr.pool, sizeof (pcp->hdr.pool),
					&pcp->hdr.pool.checksum, 0)) {
			outv(2, "pool header checksum correct\n");
			return CHECK_RESULT_CONSISTENT;
		} else {
			outv(1, "incorrect pool header checksum\n");
			if (!pcp->repair)
				return CHECK_RESULT_NOT_CONSISTENT;
			util_convert2h_pool_hdr(&pcp->hdr.pool);
		}
	} else {
		outv(1, "pool_hdr zeroed\n");
		if (!pcp->repair)
			return CHECK_RESULT_NOT_CONSISTENT;
	}

	/*
	 * Here we have invalid checksum or zeroed pool_hdr
	 * and repair flag is set so we can try to repair the
	 * pool_hdr structure by writing some default values.
	 */

	struct pool_hdr default_hdr;

	if (pcp->ptype == PMEM_POOL_TYPE_UNKNOWN) {
		/*
		 * We can scan pool file for valid BTT Info header
		 * if found it means this is pmem blk pool.
		 */
		if (pmempool_check_get_first_valid_arena(pcp,
					&pcp->bttc)) {
			outv(1, "pool_hdr.signature is not valid\n");
			if (ask_Yn(pcp->ans, "Do you want to set "
				"pool_hdr.signature to %s?", BLK_HDR_SIG)
					== 'y') {
				outv(1, "setting pool_hdr.signature to %s\n",
						BLK_HDR_SIG);
				strncpy(pcp->hdr.pool.signature, BLK_HDR_SIG,
						POOL_HDR_SIG_LEN);
				pcp->ptype = PMEM_POOL_TYPE_BLK;
			} else {
				return CHECK_RESULT_CANNOT_REPAIR;
			}
		} else {
			outv(1, "cannot determine pool type\n");
			return CHECK_RESULT_CANNOT_REPAIR;
		}
	}

	/* set constant values depending on pool type */
	if (pcp->ptype == PMEM_POOL_TYPE_LOG) {
		default_hdr.major = LOG_FORMAT_MAJOR;
		default_hdr.compat_features = LOG_FORMAT_COMPAT;
		default_hdr.incompat_features = LOG_FORMAT_INCOMPAT;
		default_hdr.ro_compat_features = LOG_FORMAT_RO_COMPAT;
	} else if (pcp->ptype == PMEM_POOL_TYPE_BLK) {
		default_hdr.major = BLK_FORMAT_MAJOR;
		default_hdr.compat_features = BLK_FORMAT_COMPAT;
		default_hdr.incompat_features = BLK_FORMAT_INCOMPAT;
		default_hdr.ro_compat_features = BLK_FORMAT_RO_COMPAT;
	} else {
		out_err("Unsupported pool type '%s'",
				out_get_pool_type_str(pcp->ptype));
		return CHECK_RESULT_ERROR;
	}

	if (pcp->hdr.pool.major != default_hdr.major) {
		outv(1, "pool_hdr.major is not valid\n");
		if (ask_Yn(pcp->ans, "Do you want to set it to default value "
				"0x%x?", default_hdr.major) == 'y') {
			outv(1, "setting pool_hdr.major to 0x%x\n",
					default_hdr.major);
			pcp->hdr.pool.major = default_hdr.major;
		}
	}

	if (pcp->hdr.pool.compat_features != default_hdr.compat_features) {
		outv(1, "pool_hdr.compat_features is not valid\n");
		if (ask_Yn(pcp->ans, "Do you want to set it to default value "
			"0x%x?", default_hdr.compat_features) == 'y') {
			outv(1, "setting pool_hdr.compat_features to 0x%x\n",
					default_hdr.compat_features);
			pcp->hdr.pool.compat_features =
					default_hdr.compat_features;
		}
	}

	if (pcp->hdr.pool.incompat_features != default_hdr.incompat_features) {
		outv(1, "pool_hdr.incompat_features is not valid\n");
		if (ask_Yn(pcp->ans, "Do you want to set it to default value "
			"0x%x?", default_hdr.incompat_features) == 'y') {
			outv(1, "setting pool_hdr.incompat_features to 0x%x\n",
					default_hdr.incompat_features);
			pcp->hdr.pool.incompat_features =
					default_hdr.incompat_features;
		}
	}

	if (pcp->hdr.pool.ro_compat_features !=
			default_hdr.ro_compat_features) {
		outv(1, "pool_hdr.ro_compat_features is not valid\n");
		if (ask_Yn(pcp->ans, "Do you want to set it to default value "
			"0x%x?", default_hdr.ro_compat_features) == 'y') {
			outv(1, "setting pool_hdr.ro_compat_features to 0x%x\n",
					default_hdr.ro_compat_features);
			pcp->hdr.pool.ro_compat_features =
				default_hdr.ro_compat_features;
		}
	}

	if (util_check_memory(pcp->hdr.pool.unused,
				sizeof (pcp->hdr.pool.unused), 0)) {
		outv(1, "unused area is not filled by zeros\n");
		if (ask_Yn(pcp->ans, "Do you want to fill it up?") == 'y') {
			outv(1, "setting pool_hdr.unused to zeros\n");
			memset(pcp->hdr.pool.unused, 0,
					sizeof (pcp->hdr.pool.unused));
		}
	}

	/* for blk pool we can take the UUID from BTT Info header */
	if (pcp->ptype == PMEM_POOL_TYPE_BLK &&
		pcp->bttc.valid &&
		memcmp(pcp->hdr.pool.uuid, pcp->bttc.btt_info.parent_uuid,
				POOL_HDR_UUID_LEN)) {
		outv(1, "UUID is not valid\n");
		if (ask_Yn(pcp->ans, "Do you want to set it to %s from "
			"BTT Info?", out_get_uuid_str(
				pcp->bttc.btt_info.parent_uuid)) == 'y') {
			outv(1, "setting pool_hdr.uuid to %s\n",
				out_get_uuid_str(
				pcp->bttc.btt_info.parent_uuid));
			memcpy(pcp->hdr.pool.uuid,
				pcp->bttc.btt_info.parent_uuid,
				POOL_HDR_UUID_LEN);
			pcp->uuid_op = UUID_FROM_BTT;
		}
	}

	util_convert2le_pool_hdr(&pcp->hdr.pool);

	if (!util_checksum(&pcp->hdr.pool, sizeof (pcp->hdr.pool),
				&pcp->hdr.pool.checksum, 0)) {

		util_convert2h_pool_hdr(&pcp->hdr.pool);

		/*
		 * Here we are sure all fields are valid except:
		 * - crtime
		 * - UUID
		 * - checksum
		 * These values are not crucial for consistency checking, so we
		 * can regenerate these values and check consistency one more
		 * time.
		 *
		 * Creation time should be less than file's modification time -
		 * if it's not it is probably corrupted. We cannot determine
		 * valid value but we can set this value to file's modification
		 * time.
		 *
		 * The UUID may be either regenerated or restored from valid
		 * BTT Info header in case of pmem blk pool.
		 * The regenerated UUID must be propagated to all BTT Info
		 * headers.
		 *
		 * Finally the checksum may be computed using fletcher64 and
		 * stored in header.
		 */
		struct stat buff;
		if (fstat(pcp->fd, &buff))
			return CHECK_RESULT_ERROR;

		if (pcp->hdr.pool.crtime > buff.st_mtime) {
			outv(1, "pool_hdr.crtime is not valid\n");
			if (ask_Yn(pcp->ans, "Do you want to set it to file's "
				"modtime [%s]?", out_get_time_str(
					buff.st_mtime)) == 'y') {
				outv(1, "setting pool_hdr.crtime to file's "
					"modtime: %s\n",
					out_get_time_str(buff.st_mtime));
				pcp->hdr.pool.crtime = buff.st_mtime;
			} else {
				return CHECK_RESULT_CANNOT_REPAIR;
			}
		}

		if (pcp->uuid_op == UUID_NOP &&
			ask_Yn(pcp->ans, "Do you want to regenerate UUID?")
				== 'y') {
			uuid_generate(pcp->hdr.pool.uuid);
			outv(1, "setting pool_hdr.uuid to: %s\n",
				out_get_uuid_str(pcp->hdr.pool.uuid));
			pcp->uuid_op = UUID_REGENERATED;
		} else {
			return CHECK_RESULT_CANNOT_REPAIR;
		}

		util_convert2le_pool_hdr(&pcp->hdr.pool);

		if (ask_Yn(pcp->ans, "Do you want to regenerate checksum?")
				== 'n')
			return CHECK_RESULT_CANNOT_REPAIR;

		util_checksum(&pcp->hdr.pool, sizeof (pcp->hdr.pool),
					&pcp->hdr.pool.checksum, 1);
		outv(1, "setting pool_hdr.checksum to: 0x%x\n",
				le32toh(pcp->hdr.pool.checksum));

		util_convert2h_pool_hdr(&pcp->hdr.pool);

		return CHECK_RESULT_REPAIRED;
	}

	return CHECK_RESULT_REPAIRED;
}

/*
 * pmempool_check_read_pmemlog -- read pmemlog header
 */
static int
pmempool_check_read_pmemlog(struct pmempool_check *pcp)
{
	/*
	 * Here we want to read the pmemlog header
	 * without the pool_hdr as we've already done it
	 * before.
	 *
	 * Take the pointer to fields right after pool_hdr,
	 * compute the size and offset of remaining fields.
	 */
	uint8_t *ptr = (uint8_t *)&pcp->hdr.log;
	ptr += sizeof (pcp->hdr.log.hdr);

	size_t size = sizeof (pcp->hdr.log) - sizeof (pcp->hdr.log.hdr);
	off_t offset = sizeof (pcp->hdr.log.hdr);

	if (pread(pcp->fd, ptr, size, offset) != size) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("cannot read pmemlog structure\n");
		return -1;
	}

	/* endianness conversion */
	util_convert2h_pmemlog(&pcp->hdr.log);

	return 0;
}

/*
 * pmempool_check_read_pmemblk -- read pmemblk header
 */
static int
pmempool_check_read_pmemblk(struct pmempool_check *pcp)
{
	/*
	 * Here we want to read the pmemlog header
	 * without the pool_hdr as we've already done it
	 * before.
	 *
	 * Take the pointer to fields right after pool_hdr,
	 * compute the size and offset of remaining fields.
	 */
	uint8_t *ptr = (uint8_t *)&pcp->hdr.blk;
	ptr += sizeof (pcp->hdr.blk.hdr);

	size_t size = sizeof (pcp->hdr.blk) - sizeof (pcp->hdr.blk.hdr);
	off_t offset = sizeof (pcp->hdr.blk.hdr);

	if (pread(pcp->fd, ptr, size, offset) != size) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("cannot read pmemblk structure\n");
		return -1;
	}

	/* endianness conversion */
	pcp->hdr.blk.bsize = le32toh(pcp->hdr.blk.bsize);

	return 0;
}

/*
 * pmempool_check_pmemlog -- try to repair pmemlog header
 */
static check_result_t
pmempool_check_pmemlog(struct pmempool_check *pcp)
{
	outv(2, "checking pmemlog header\n");

	if (pmempool_check_read_pmemlog(pcp))
		return CHECK_RESULT_ERROR;

	/* determine constant values for pmemlog */
	const uint64_t d_start_offset =
		roundup(sizeof (pcp->hdr.log),
				LOG_FORMAT_DATA_ALIGN);
	struct stat buff;
	if (fstat(pcp->fd, &buff)) {
		if (errno)
			warn("%s", pcp->fname);
		return CHECK_RESULT_ERROR;
	}

	check_result_t ret = CHECK_RESULT_CONSISTENT;

	if (pcp->hdr.log.start_offset != d_start_offset) {
		outv(1, "invalid pmemlog.start_offset: 0x%x\n",
			pcp->hdr.log.start_offset);
		if (pcp->repair) {
			if (ask_Yn(pcp->ans, "Do you want to set "
				"pmemlog.start_offset to default 0x%x?",
				d_start_offset) == 'y') {
				outv(1, "setting pmemlog.start_offset"
					" to 0x%x\n", d_start_offset);
				pcp->hdr.log.start_offset = d_start_offset;

				ret = CHECK_RESULT_REPAIRED;
			} else {
				return CHECK_RESULT_CANNOT_REPAIR;
			}
		} else {
			return CHECK_RESULT_NOT_CONSISTENT;
		}
	}

	if (pcp->hdr.log.end_offset != buff.st_size) {
		outv(1, "invalid pmemlog.end_offset: 0x%x\n",
			pcp->hdr.log.end_offset);
		if (pcp->repair) {
			if (ask_Yn(pcp->ans, "Do you want to set "
				"pmemlog.end_offset to 0x%x?",
				buff.st_size) == 'y') {
				outv(1, "setting pmemlog.end_offset to 0x%x\n",
						buff.st_size);
				pcp->hdr.log.end_offset = buff.st_size;

				ret = CHECK_RESULT_REPAIRED;
			} else {
				return CHECK_RESULT_CANNOT_REPAIR;
			}
		} else {
			return CHECK_RESULT_NOT_CONSISTENT;
		}
	}

	if (pcp->hdr.log.write_offset < d_start_offset ||
	    pcp->hdr.log.write_offset > buff.st_size) {
		outv(1, "invalid pmemlog.write_offset: 0x%x\n",
			pcp->hdr.log.write_offset);
		if (pcp->repair) {
			if (ask_Yn(pcp->ans, "Do you want to set "
				"pmemlog.write_offset to pmemlog.end_offset?")
					== 'y') {
				outv(1, "setting pmemlog.write_offset "
						"to pmemlog.end_offset\n");
				pcp->hdr.log.write_offset = buff.st_size;

				ret = CHECK_RESULT_REPAIRED;
			} else {
				return CHECK_RESULT_CANNOT_REPAIR;
			}
		} else {
			return CHECK_RESULT_NOT_CONSISTENT;
		}
	}

	if (ret == CHECK_RESULT_CONSISTENT)
		outv(2, "pmemlog header correct\n");

	return ret;
}


/*
 * pmempool_check_pmemblk -- try to repair pmemblk header
 */
static check_result_t
pmempool_check_pmemblk(struct pmempool_check *pcp)
{
	outv(2, "checking pmemblk header\n");

	if (pmempool_check_read_pmemblk(pcp))
		return CHECK_RESULT_ERROR;

	/* check for valid BTT Info arena as we can take bsize from it */
	if (!pcp->bttc.valid)
		pmempool_check_get_first_valid_arena(pcp, &pcp->bttc);

	if (pcp->bttc.valid) {
		const uint32_t btt_bsize =
			pcp->bttc.btt_info.external_lbasize;

		if (pcp->hdr.blk.bsize != btt_bsize) {
			outv(1, "invalid pmemblk.bsize\n");
			if (pcp->repair) {
				if (ask_Yn(pcp->ans, "Do you want to set "
				"pmemblk.bsize to %lu from BTT Info?",
				btt_bsize) == 'y') {
					outv(1, "setting pmemblk.b_size"
						" to 0x%x\n", btt_bsize);
					pcp->hdr.blk.bsize = btt_bsize;

					return CHECK_RESULT_REPAIRED;
				} else {
					return CHECK_RESULT_CANNOT_REPAIR;
				}
			} else {
				return CHECK_RESULT_NOT_CONSISTENT;
			}
		}
	} else {
		/* cannot find valid BTT Info */
		struct stat buff;
		if (fstat(pcp->fd, &buff)) {
			if (errno)
				warn("%s", pcp->fname);
			return CHECK_RESULT_ERROR;
		}

		if (pcp->hdr.blk.bsize < BTT_MIN_LBA_SIZE ||
			util_check_bsize(pcp->hdr.blk.bsize,
				buff.st_size)) {
			outv(1, "invalid pmemblk.bsize\n");
			return CHECK_RESULT_CANNOT_REPAIR;
		}
	}

	outv(2, "pmemblk header correct\n");

	return CHECK_RESULT_CONSISTENT;
}

/*
 * pmempool_check_check_btt -- check consistency of BTT Info header
 */
static int
pmempool_check_check_btt(struct btt_info *infop)
{
	if (!memcmp(infop->sig, BTT_INFO_SIG, BTTINFO_SIG_LEN))
		return util_checksum(infop, sizeof (*infop),
				&infop->checksum, 0);
	else
		return -1;
}

/*
 * pmempool_check_btt_info_advanced_repair -- restore btt using btt API
 */
static int
pmempool_check_btt_info_advanced_repair(struct pmempool_check *pcp,
		off_t startoff, off_t endoff)
{
	bool eof = false;
	if (!endoff) {
		struct stat buff;
		if (fstat(pcp->fd, &buff))
			return -1;
		endoff = buff.st_size;
		eof = true;
	}

	outv(1, "generating BTT Info headers at 0x%lx-0x%lx\n",
			startoff, endoff);
	uint64_t rawsize = endoff - startoff;

	int flags = pcp->repair && pcp->exec ? MAP_SHARED : MAP_PRIVATE;

	/*
	 * Map the whole requested area in private mode as we want to write
	 * only BTT Info headers to file.
	 */
	void *addr = mmap(NULL, rawsize, PROT_READ|PROT_WRITE, flags,
			pcp->fd, startoff);

	if (addr == MAP_FAILED) {
		warn("%s", pcp->fname);
		return -1;
	}

	int ret = 0;

	/* setup btt context */
	struct btt_context btt_context = {
		.pcp = pcp,
		.addr = addr,
		.len = rawsize
	};

	uint32_t lbasize = pcp->hdr.blk.bsize;

	/* init btt in requested area */
	struct btt *bttp = btt_init(rawsize,
				lbasize, pcp->hdr.pool.uuid,
				BTT_DEFAULT_NFREE,
				(void *)&btt_context,
				&pmempool_check_btt_ns_callback);

	if (!bttp) {
		out_err("cannot initialize BTT layer\n");
		ret = -1;
		goto error;
	}

	/* lazy layout writing */
	if (btt_write(bttp, 0, 0, addr)) {
		out_err("writing layout failed\n");
		ret = -1;
		goto error_btt;
	}

	/* add all arenas to list */
	struct arena *arenap = NULL;
	uint64_t offset = 0;
	uint64_t nextoff = 0;
	do {
		offset += nextoff;
		struct btt_info *infop =
			(struct btt_info *)((uintptr_t)addr + offset);

		if (pmempool_check_check_btt(infop) != 1) {
			ret = -1;
			goto error_btt;
		}
		arenap = malloc(sizeof (struct arena));
		if (!arenap)
			err(1, "Cannot allocate memory for arena");
		memset(arenap, 0, sizeof (*arenap));
		arenap->offset = offset + startoff;
		arenap->valid = true;
		arenap->id = pcp->narenas;
		memcpy(&arenap->btt_info, infop, sizeof (arenap->btt_info));

		pmempool_check_insert_arena(pcp, arenap);

		nextoff = le64toh(infop->nextoff);
	} while (nextoff > 0);

	if (!eof) {
		/*
		 * It means that requested area is between two valid arenas
		 * so make sure the offsets are correct.
		 */
		nextoff = endoff - (startoff + offset);
		if (nextoff != le64toh(arenap->btt_info.infooff) +
				sizeof (arenap->btt_info)) {
			ret = -1;
			goto error_btt;
		} else {
			arenap->btt_info.nextoff = htole64(nextoff);
			util_checksum(&arenap->btt_info,
					sizeof (arenap->btt_info),
					&arenap->btt_info.checksum, 1);
		}

	}

error_btt:
	btt_fini(bttp);
error:
	munmap(addr, rawsize);
	return ret;
}

/*
 * pmempool_check_btt_info -- try to repair BTT Info Headers
 */
static check_result_t
pmempool_check_btt_info(struct pmempool_check *pcp)
{
	outv(2, "checking BTT Info headers\n");
	/*
	 * For pmem blk pool type this is the constant offset at which BTT Info
	 * header of arena 0 should be.
	 */
	uint64_t offset = 2 * BTT_ALIGNMENT;
	uint64_t nextoff = 0;
	ssize_t ret = 0;
	check_result_t result = CHECK_RESULT_CONSISTENT;
	do {
		struct arena *arenap = calloc(1, sizeof (struct arena));
		if (!arenap)
			err(1, "Cannot allocate memory for arena");
		offset += nextoff;

		/* read the BTT Info header at well known offset */
		if ((ret = pread(pcp->fd, &arenap->btt_info,
			sizeof (arenap->btt_info), offset)) !=
					sizeof (arenap->btt_info)) {
			if (errno)
				warn("%s", pcp->fname);
			out_err("arena %u: cannot read BTT Info header\n",
					arenap->id);
			free(arenap);
			return CHECK_RESULT_ERROR;
		}

		arenap->id = pcp->narenas;

		off_t advanced_repair_endoff = 0;
		bool advanced_repair = false;

		if (util_check_memory((const uint8_t *)&arenap->btt_info,
				sizeof (arenap->btt_info), 0) == 0) {
			outv(2, "BTT Layout not written\n");
			free(arenap);
			return CHECK_RESULT_CONSISTENT_BREAK;
		}
		/* check consistency of BTT Info */
		int ret = pmempool_check_check_btt(&arenap->btt_info);

		if (ret == 1)
			outv(2, "arena %u: BTT Info header checksum"
					" correct\n", arenap->id);
		else {
			outv(1, "arena %u: BTT Info header checksum"
					" incorrect\n", arenap->id);
			if (!pcp->repair) {
				free(arenap);
				return CHECK_RESULT_NOT_CONSISTENT;
			}
		}

		if (ret != 1 && pcp->repair) {
			/*
			 * BTT Info header is not consistent, so try to find
			 * backup first.
			 *
			 * BTT Info header backup is in the last page of arena,
			 * we know the BTT Info size and arena minimum size so
			 * we can start searching at some higher offset.
			 */
			off_t search_off = offset + BTT_MIN_SIZE -
				sizeof (struct btt_info);

			off_t b_off = 0;
			/*
			 * Read first valid BTT Info to bttc buffer
			 * check whether this BTT Info header is the
			 * backup by checking offset value.
			 */
			if ((b_off = pmempool_check_get_first_valid_btt(pcp,
				&pcp->bttc.btt_info, search_off)) &&
				offset + pcp->bttc.btt_info.infooff == b_off) {
				/*
				 * Here we have valid BTT Info backup
				 * so we can restore it.
				 */
				if (ask_Yn(pcp->ans, "Restore from backup?")
						== 'n') {
					free(arenap);
					return CHECK_RESULT_CANNOT_REPAIR;
				}
				outv(1, "arena %u: restoring BTT Info"
					" header from backup\n", arenap->id);

				memcpy(&arenap->btt_info,
					&pcp->bttc.btt_info,
					sizeof (arenap->btt_info));
				advanced_repair = false;

				result = CHECK_RESULT_REPAIRED;
			} else {
				advanced_repair = true;
				advanced_repair_endoff = b_off;
			}
		}

		/*
		 * If recovering by BTT Info backup failed try to
		 * regenerate btt layout.
		 */
		if (pcp->repair && advanced_repair) {
			if (ask_Yn(pcp->ans,
				"Do you want to restore BTT layout?") == 'n' ||
				pmempool_check_btt_info_advanced_repair(pcp,
				offset, advanced_repair_endoff)) {
				outv(1, "arena %u: cannot repair BTT Info"
					" header\n", arenap->id);
				free(arenap);

				return CHECK_RESULT_CANNOT_REPAIR;
			} else {
				ret = CHECK_RESULT_REPAIRED;
			}
		}

		if (pcp->repair && advanced_repair) {
			if (advanced_repair_endoff)
				nextoff = advanced_repair_endoff - offset;
			else
				nextoff = 0;
			free(arenap);
		} else {
			/* save offset and insert BTT to cache for next steps */
			arenap->offset = offset;
			arenap->valid = true;
			pmempool_check_insert_arena(pcp, arenap);
			nextoff = le64toh(arenap->btt_info.nextoff);
		}
	} while (nextoff > 0 && ret >= 0);

	return result;
}

/*
 * pmempool_check_check_flog -- return valid flog entry
 */
static struct btt_flog *
pmempool_check_check_flog(struct arena *arenap, struct btt_flog *flog_alpha,
		struct btt_flog *flog_beta)
{
	struct btt_flog *ret = NULL;

	/*
	 * Valid seq numbers are 1 or 2.
	 * The interesting cases are:
	 * - no valid seq numbers:  layout consistency error
	 * - one valid seq number:  that's the current entry
	 * - two valid seq numbers: higher number is current entry
	 * - identical seq numbers: layout consistency error
	 *
	 * The following is the combination of two seq numbers.
	 * The higher byte is the first half of flog entry and
	 * the lower byte is the second half of flog entry.
	 *
	 * The valid values though are:
	 * - 0x10, 0x21 - the first half is current entry
	 * - 0x01, 0x12 - the second half is current entry
	 */
	uint8_t seqc = ((flog_alpha->seq & 0xf) << 4) |
			(flog_beta->seq & 0xf);
	switch (seqc) {
	case 0x10:
	case 0x21:
		ret = flog_alpha;
		break;
	case 0x01:
	case 0x12:
		ret = flog_beta;
		break;
	default:
		ret = NULL;
	}

	return ret;
}

/*
 * pmempool_check_write_flog -- convert and write flog to file
 */
static int
pmempool_check_write_flog(struct pmempool_check *pcp, struct arena *arenap)
{
	if (!arenap->flog)
		return -1;

	uint64_t flogoff = arenap->offset + arenap->btt_info.flogoff;

	uint8_t *ptr = arenap->flog;
	int i;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		struct btt_flog *flog_alpha = (struct btt_flog *)ptr;
		struct btt_flog *flog_beta = (struct btt_flog *)(ptr +
				sizeof (struct btt_flog));

		util_convert2le_btt_flog(flog_alpha);
		util_convert2le_btt_flog(flog_beta);

		ptr += BTT_FLOG_PAIR_ALIGN;
	}

	int ret = 0;
	if (pwrite(pcp->fd, arenap->flog, arenap->flogsize, flogoff) !=
			arenap->flogsize) {
		if (errno)
			warn("%s", pcp->fname);
		ret = -1;
	}

	if (fsync(pcp->fd)) {
		warn("%s", pcp->fname);
		ret = -1;
	}

	if (ret)
		out_err("arena %u: writing BTT FLOG failed\n", arenap->id);

	return 0;
}

/*
 * pmempool_check_read_flog -- read and convert flog from file
 */
static int
pmempool_check_read_flog(struct pmempool_check *pcp, struct arena *arenap)
{
	uint64_t flogoff = arenap->offset + arenap->btt_info.flogoff;

	uint64_t flogsize = arenap->btt_info.nfree *
		roundup(2 * sizeof (struct btt_flog),
				BTT_FLOG_PAIR_ALIGN);
	arenap->flogsize = roundup(flogsize, BTT_ALIGNMENT);

	arenap->flog = malloc(arenap->flogsize);
	if (!arenap->flog)
		err(1, "Cannot allocate memory for FLOG entries");

	if (pread(pcp->fd, arenap->flog, arenap->flogsize, flogoff)
			!= arenap->flogsize) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("arena %u: cannot read BTT FLOG\n", arenap->id);
		return -1;
	}

	uint8_t *ptr = arenap->flog;
	int i;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		struct btt_flog *flog_alpha = (struct btt_flog *)ptr;
		struct btt_flog *flog_beta = (struct btt_flog *)(ptr +
				sizeof (struct btt_flog));

		util_convert2h_btt_flog(flog_alpha);
		util_convert2h_btt_flog(flog_beta);

		ptr += BTT_FLOG_PAIR_ALIGN;
	}


	return 0;
}

/*
 * pmempool_check_write_map -- convert and write map to file
 */
static int
pmempool_check_write_map(struct pmempool_check *pcp, struct arena *arenap)
{
	if (!arenap->map)
		return -1;

	uint64_t mapoff = arenap->offset + arenap->btt_info.mapoff;

	int i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++)
		arenap->map[i] = htole32(arenap->map[i]);

	int ret = 0;
	if (pwrite(pcp->fd, arenap->map, arenap->mapsize, mapoff) !=
			arenap->mapsize) {
		if (errno)
			warn("%s", pcp->fname);
		ret = -1;
	}

	if (fsync(pcp->fd)) {
		warn("%s", pcp->fname);
		ret = -1;
	}

	if (ret)
		out_err("arena %u: writing BTT map failed\n", arenap->id);

	return ret;
}

/*
 * pmempool_check_read_map -- read and convert map from file
 */
static int
pmempool_check_read_map(struct pmempool_check *pcp, struct arena *arenap)
{
	uint64_t mapoff = arenap->offset + arenap->btt_info.mapoff;

	arenap->mapsize = roundup(
			arenap->btt_info.external_nlba *
			BTT_MAP_ENTRY_SIZE, BTT_ALIGNMENT);

	arenap->map = malloc(arenap->mapsize);
	if (!arenap->map)
		err(1, "Cannot allocate memory for BTT map");

	if (pread(pcp->fd, arenap->map, arenap->mapsize, mapoff) !=
			arenap->mapsize) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("arena %u: cannot read BTT map\n", arenap->id);
		return -1;
	}

	int i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++)
		arenap->map[i] = le32toh(arenap->map[i]);

	return 0;
}

/*
 * pmempool_check_arena_map_flog -- try to repair map and flog
 */
static check_result_t
pmempool_check_arena_map_flog(struct pmempool_check *pcp,
		struct arena *arenap)
{
	check_result_t ret = 0;

	/* read flog and map entries */
	if (pmempool_check_read_flog(pcp, arenap))
		return CHECK_RESULT_ERROR;

	if (pmempool_check_read_map(pcp, arenap))
		return CHECK_RESULT_ERROR;

	/* create bitmap for checking duplicated blocks */
	uint32_t bitmapsize = howmany(arenap->btt_info.internal_nlba, 8);
	char *bitmap = malloc(bitmapsize);
	if (!bitmap)
		err(1, "Cannot allocate memory for blocks bitmap");
	memset(bitmap, 0, bitmapsize);

	/* list of invalid map entries */
	struct list *list_inval = list_alloc();
	/* list of invalid flog entries */
	struct list *list_flog_inval = list_alloc();
	/* list of unmapped blocks */
	struct list *list_unmap = list_alloc();

	/* check map entries */
	uint32_t i;
	for (i = 0; i < arenap->btt_info.external_nlba; i++) {
		uint32_t entry = arenap->map[i];
		if ((entry & ~BTT_MAP_ENTRY_LBA_MASK) == 0)
			entry = i;
		else
			entry &= BTT_MAP_ENTRY_LBA_MASK;

		/* add duplicated and invalid entries to list */
		if (entry < arenap->btt_info.internal_nlba) {
			if (isset(bitmap, entry)) {
				outv(1, "arena %u: map entry %u duplicated "
					"at %u\n", arenap->id, entry, i);
				list_push(list_inval, i);
			} else {
				setbit(bitmap, entry);
			}
		} else {
			outv(1, "arena %u: invalid map entry at %u\n",
				arenap->id, i);
			list_push(list_inval, i);
		}
	}

	/* check flog entries */
	uint8_t *ptr = arenap->flog;
	for (i = 0; i < arenap->btt_info.nfree; i++) {
		/* first and second copy of flog entry */
		struct btt_flog *flog_alpha = (struct btt_flog *)ptr;
		struct btt_flog *flog_beta = (struct btt_flog *)(ptr +
				sizeof (struct btt_flog));

		/*
		 * Check flog entry and return current one
		 * by checking sequence number.
		 */
		struct btt_flog *flog_cur =
			pmempool_check_check_flog(arenap, flog_alpha,
					flog_beta);

		/* insert invalid and duplicated indexes to list */
		if (flog_cur) {
			uint32_t entry = flog_cur->old_map &
				BTT_MAP_ENTRY_LBA_MASK;

			if (isset(bitmap, entry)) {
				outv(1, "arena %u: duplicated flog entry "
					"at %u\n", arenap->id, entry, i);
				list_push(list_flog_inval, i);
			} else {
				setbit(bitmap, entry);
			}
		} else {
			outv(1, "arena %u: invalid flog entry at %u\n",
				arenap->id, i);
			list_push(list_flog_inval, i);
		}

		ptr += BTT_FLOG_PAIR_ALIGN;
	}

	/* check unmapped blocks and insert to list */
	for (i = 0; i < arenap->btt_info.internal_nlba; i++) {
		if (!isset(bitmap, i)) {
			outv(1, "arena %u: unmapped block %u\n", arenap->id, i);
			list_push(list_unmap, i);
		}
	}

	if (list_unmap->count)
		outv(1, "arena %u: number of unmapped blocks: %u\n",
				arenap->id, list_unmap->count);
	if (list_inval->count)
		outv(1, "arena %u: number of invalid map entries: %u\n",
				arenap->id, list_inval->count);
	if (list_flog_inval->count)
		outv(1, "arena %u: number of invalid flog entries: %u\n",
				arenap->id, list_flog_inval->count);

	if (!pcp->repair && list_unmap->count > 0) {
		ret = CHECK_RESULT_NOT_CONSISTENT;
		goto out;
	}

	/*
	 * We are able to repair if and only if number of unmapped blocks is
	 * equal to sum of invalid map and flog entries.
	 */
	if (list_unmap->count != (list_inval->count + list_flog_inval->count)) {
		outv(1, "arena %u: cannot repair map and flog\n",
			arenap->id);
		ret = CHECK_RESULT_CANNOT_REPAIR;
		goto out;
	}

	if (list_inval->count > 0 &&
		ask_Yn(pcp->ans,
		"Do you want repair invalid map entries ?") == 'n') {
		ret = CHECK_RESULT_CANNOT_REPAIR;
		goto out;
	}

	if (list_flog_inval->count > 0 &&
		ask_Yn(pcp->ans,
		"Do you want to repair invalid flog entries ?") == 'n') {
		ret = CHECK_RESULT_CANNOT_REPAIR;
		goto out;
	}
	/*
	 * Repair invalid or duplicated map entries
	 * by using unmapped blocks.
	 */
	uint32_t inval;
	uint32_t unmap;
	while (list_pop(list_inval, &inval)) {
		if (!list_pop(list_unmap, &unmap)) {
			ret = CHECK_RESULT_ERROR;
			goto out;
		}
		arenap->map[inval] = unmap | BTT_MAP_ENTRY_ERROR;
		outv(1, "arena %u: storing 0x%x at %u entry\n",
				arenap->id, arenap->map[inval], inval);
	}

	/* repair invalid flog entries using unmapped blocks */
	while (list_pop(list_flog_inval, &inval)) {
		if (!list_pop(list_unmap, &unmap)) {
			ret = CHECK_RESULT_ERROR;
			goto out;
		}

		struct btt_flog *flog_alpha = (struct btt_flog *)(arenap->flog +
				inval * BTT_FLOG_PAIR_ALIGN);
		struct btt_flog *flog_beta = (struct btt_flog *)(arenap->flog +
				inval * BTT_FLOG_PAIR_ALIGN +
				sizeof (struct btt_flog));
		memset(flog_beta, 0, sizeof (*flog_beta));
		uint32_t entry = unmap | BTT_MAP_ENTRY_ERROR;
		flog_alpha->lba = 0;
		flog_alpha->new_map = entry;
		flog_alpha->old_map = entry;
		flog_alpha->seq = 1;

		outv(1, "arena %u: repairing flog at %u with free block "
			"entry 0x%x\n", arenap->id, inval, entry);
	}

out:
	list_free(list_inval);
	list_free(list_flog_inval);
	list_free(list_unmap);
	free(bitmap);

	return ret;
}

/*
 * pmempool_check_btt_map_flog -- try to repair BTT maps and flogs
 */
static check_result_t
pmempool_check_btt_map_flog(struct pmempool_check *pcp)
{
	outv(2, "checking BTT map and flog\n");

	check_result_t ret = CHECK_RESULT_ERROR;
	uint32_t narena = 0;
	struct arena *arenap;
	TAILQ_FOREACH(arenap, &pcp->arenas, next) {
		outv(2, "arena %u: checking map and flog\n", narena);
		ret = pmempool_check_arena_map_flog(pcp, arenap);
		if (ret != CHECK_RESULT_CONSISTENT &&
			ret != CHECK_RESULT_CONSISTENT) {
			break;
		}
		narena++;
	}

	return ret;
}

/*
 * pmempool_check_write_log -- write all structures for log pool
 */
static check_result_t
pmempool_check_write_log(struct pmempool_check *pcp)
{
	if (!pcp->repair || !pcp->exec)
		return 0;

	/* endianness conversion */
	pcp->hdr.log.start_offset = htole64(pcp->hdr.log.start_offset);
	pcp->hdr.log.end_offset = htole64(pcp->hdr.log.end_offset);
	pcp->hdr.log.write_offset = htole64(pcp->hdr.log.write_offset);

	int ret = 0;

	if (pwrite(pcp->fd, &pcp->hdr.log, sizeof (pcp->hdr.log), 0)
		!= sizeof (pcp->hdr.log)) {
		if (errno)
			warn("%s", pcp->fname);
		ret = -1;
	}

	if (fsync(pcp->fd)) {
		warn("%s", pcp->fname);
		ret = -1;
	}

	if (ret)
		out_err("writing pmemlog structure failed\n");

	return ret;
}

/*
 * pmempool_check_write_blk -- write all structures for blk pool
 */
static check_result_t
pmempool_check_write_blk(struct pmempool_check *pcp)
{
	if (!pcp->repair || !pcp->exec)
		return 0;

	/* endianness conversion */
	pcp->hdr.blk.bsize = htole32(pcp->hdr.blk.bsize);

	if (pwrite(pcp->fd, &pcp->hdr.blk,
		sizeof (pcp->hdr.blk), 0)
		!= sizeof (pcp->hdr.blk)) {
		if (errno)
			warn("%s", pcp->fname);
		out_err("writing pmemblk structure failed\n");
		return -1;
	}

	struct arena *arenap;
	TAILQ_FOREACH(arenap, &pcp->arenas, next) {

		util_convert2le_btt_info(&arenap->btt_info);

		if (pcp->uuid_op == UUID_REGENERATED) {
			memcpy(arenap->btt_info.parent_uuid, pcp->hdr.pool.uuid,
					sizeof (arenap->btt_info.parent_uuid));

			util_checksum(&arenap->btt_info,
					sizeof (arenap->btt_info),
					&arenap->btt_info.checksum, 1);
		}

		if (pwrite(pcp->fd, &arenap->btt_info,
			sizeof (arenap->btt_info), arenap->offset) !=
			sizeof (arenap->btt_info)) {
			if (errno)
				warn("%s", pcp->fname);
			out_err("arena %u: writing BTT Info failed\n",
				arenap->id);
			return -1;
		}

		if (pwrite(pcp->fd, &arenap->btt_info,
			sizeof (arenap->btt_info), arenap->offset +
				le64toh(arenap->btt_info.infooff)) !=
			sizeof (arenap->btt_info)) {
			if (errno)
				warn("%s", pcp->fname);
			out_err("arena %u: writing BTT Info backup failed\n",
				arenap->id);
		}

		if (pmempool_check_write_flog(pcp, arenap))
			return -1;

		if (pmempool_check_write_map(pcp, arenap))
			return -1;
	}

	if (fsync(pcp->fd)) {
		warn("%s", pcp->fname);
		return -1;
	}

	return 0;
}
/*
 * pmempool_check_steps -- check steps
 */
static const struct pmempool_check_step pmempool_check_steps[] = {
	{
		.type	= PMEM_POOL_TYPE_BLK
				| PMEM_POOL_TYPE_LOG
				| PMEM_POOL_TYPE_UNKNOWN,
		.func	= pmempool_check_pool_hdr,
	},
	{
		.type	= PMEM_POOL_TYPE_LOG,
		.func	= pmempool_check_pmemlog,
	},
	{
		.type	= PMEM_POOL_TYPE_BLK,
		.func	= pmempool_check_pmemblk,
	},
	{
		.type	= PMEM_POOL_TYPE_BLK,
		.func	= pmempool_check_btt_info,
	},
	{
		.type	= PMEM_POOL_TYPE_BLK,
		.func	= pmempool_check_btt_map_flog,
	},
	{
		.type	= PMEM_POOL_TYPE_LOG,
		.func	= pmempool_check_write_log,
	},
	{
		.type	= PMEM_POOL_TYPE_BLK,
		.func	= pmempool_check_write_blk,
	},
	{
		.func	= NULL,
	},
};

/*
 * pmempool_check_single_step -- run single step
 *
 * Returns non-zero if processing should be stopped, otherwise
 * returns zero.
 */
static int
pmempool_check_single_step(struct pmempool_check *pcp,
		const struct pmempool_check_step *step, check_result_t *resp)
{
	if (step->func == NULL)
		return 1;

	if (!(step->type & pcp->ptype))
		return 0;

	check_result_t ret = step->func(pcp);

	switch (ret) {
	case CHECK_RESULT_CONSISTENT:
		return 0;
	case CHECK_RESULT_REPAIRED:
		*resp = ret;
		return 0;
	case CHECK_RESULT_NOT_CONSISTENT:
		*resp = ret;
		/*
		 * don't continue if pool is not consistent
		 * and we don't want to repair
		 */
		return !pcp->repair;
	case CHECK_RESULT_CONSISTENT_BREAK:
	case CHECK_RESULT_CANNOT_REPAIR:
	case CHECK_RESULT_ERROR:
		*resp = ret;
		return 1;
	default:
		return 1;
	}
}

/*
 * pmempool_check_repair -- run all repair steps
 */
static check_result_t
pmempool_check_all_steps(struct pmempool_check *pcp)
{
	if (pcp->repair && pcp->backup && pcp->exec) {
		if (pmempool_check_create_backup(pcp)) {
			out_err("unable to create backup file\n");
			return -1;
		}
	}

	int flags = pcp->repair ? O_RDWR : O_RDONLY;
	if ((pcp->fd = open(pcp->fname, flags)) < 0) {
		warn("%s", pcp->fname);
		return -1;
	}

	check_result_t ret = CHECK_RESULT_CONSISTENT;
	int i = 0;
	while (!pmempool_check_single_step(pcp,
			&pmempool_check_steps[i++], &ret));

	close(pcp->fd);

	return ret;
}

/*
 * pmempool_check_func -- main function for check command
 */
int
pmempool_check_func(char *appname, int argc, char *argv[])
{
	int ret = 0;
	check_result_t res = CHECK_RESULT_CONSISTENT;
	struct pmempool_check pc = pmempool_check_default;
	TAILQ_INIT(&pc.arenas);

	/* parse command line arguments */
	ret = pmempool_check_parse_args(&pc, appname, argc, argv);
	if (ret)
		return ret;

	/* set verbosity level */
	out_set_vlevel(pc.verbose);

	struct pmem_pool_params params;
	if (pmem_pool_parse_params(pc.fname, &params)) {
		perror(pc.fname);
		return -1;
	}

	pc.ptype = params.type;

	/*
	 * The PMEM_POOL_TYPE_NONE means no such file or
	 * cannot read file so it doesn't make any sense
	 * to call pmem*_check().
	 */
	if (pc.ptype == PMEM_POOL_TYPE_NONE) {
		warn("%s", pc.fname);
		ret = -1;
	} else {
		res = pmempool_check_all_steps(&pc);
		switch (res) {
		case CHECK_RESULT_CONSISTENT:
		case CHECK_RESULT_CONSISTENT_BREAK:
			outv(2, "%s: consistent\n", pc.fname);
			ret = 0;
			break;
		case CHECK_RESULT_NOT_CONSISTENT:
			outv(1, "%s: not consistent\n", pc.fname);
			ret = -1;
			break;
		case CHECK_RESULT_REPAIRED:
			outv(1, "%s: repaired\n", pc.fname);
			ret = 0;
			break;
		case CHECK_RESULT_CANNOT_REPAIR:
			outv(1, "%s: cannot repair\n", pc.fname);
			ret = -1;
			break;
		case CHECK_RESULT_ERROR:
			out_err("repairing failed\n");
			ret = -1;
			break;
		}
	}

	pmempool_check_clear_arenas(&pc);

	return ret;
}
