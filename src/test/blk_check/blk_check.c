/*
 * Copyright (c) 2014, Intel Corporation
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
 * blk_check.c -- unit test for pmemblk_check
 *
 * The test is deliberately large, because the set_up is a costly
 * function due to the initial write.
 *
 * usage: blk_check bsize file
 */

#include "unittest.h"
#include <sys/param.h>

/* include the internal layout of the pmemblk pool */
#include "util.h"
#include "blk.h"
#include "btt_layout.h"

static const size_t block_size = 512;
static char *file_name;
extern unsigned long Pagesize;

/*
 * construct -- build a buffer for writing
 */
void
construct(int *ordp, unsigned char *buf)
{
	for (int i = 0; i < block_size; i++)
		buf[i] = *ordp;

	(*ordp)++;

	if (*ordp > 255)
		*ordp = 1;
}

/*
 * mapped_read -- read data under given offset
 */
static int
mapped_read(void *ns, void *buf, size_t count, off_t off)
{
	memcpy(buf, ns + off, count);
	return 0;
}

/*
 * mapped_write -- write data under given offset
 */
static int
mapped_write(void *ns, const void *buf, size_t count, off_t off)
{
	void *dest = ns + off;

	memcpy(dest, buf, count);

	uintptr_t uptr;

	/*
	 * msync requires uptr to be a multiple of pagesize, so adjust
	 * uptr to be aligned to the pagesize and add the difference
	 * from aligning to the lenght of data.
	 */

	/* increase len by the amount we gain when we round addr down */
	count += (uintptr_t)dest & (Pagesize - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)dest & ~(Pagesize - 1);

	if (msync((void *)uptr, count, MS_SYNC) < 0)
		FATAL("!msync");

	return 0;
}

/*
 * check_consistency -- check consistency of the tested pmemblk pool
 */
void
check_consistency()
{
	int result = pmemblk_pool_check(file_name);
	if (result < 0)
		OUT("!%s: pmemblk_check", file_name);
	else if (result == 0)
		OUT("%s: pmemblk_check: not consistent", file_name);
	else
		OUT("%s: pmemblk_check: consistent", file_name);
}

int
main(int argc, char *argv[])
{
	/* set up the test */
	START(argc, argv, "blk_check");

	PMEMblkpool *handle;

	util_init(); /* initialize Pagesize */

	if (argc != 3)
		FATAL("usage: %s file", argv[0]);

	/* get file name */
	file_name = argv[1];

	/* perform one arbitrary write to write layout */
	if ((handle = pmemblk_pool_open(file_name, block_size)) == NULL)
		FATAL("!%s: pmemblk_map", file_name);

	unsigned char write_buffer[block_size];
	int write_val = 1;
	construct(&write_val, write_buffer);

	pmemblk_write(handle, write_buffer, rand() % pmemblk_nblock(handle));
	pmemblk_pool_close(handle);

	int fd = OPEN(file_name, O_RDWR);
	struct stat stbuf;
	if (fstat(fd, &stbuf) < 0)
		FATAL("!%s: fstat", file_name);

	void *fns = MMAP(0, stbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED,
			fd, 0);

	/* change the btt_info data and verify consistency */

	off_t info_off = roundup(sizeof (struct pmemblk),
			BLK_FORMAT_DATA_ALIGN);	/* btt_info block offset */
	struct btt_info info_original;	/* original btt_info block */

	/* read the btt info block */
	mapped_read(fns, &info_original, sizeof (info_original), info_off);

	/* modifiable copy of the btt_info block */
	struct btt_info info_copy = info_original;

	/* modify fields of the btt info block one by one */

	OUT("Change btt_info.checksum");
	info_copy.checksum = htole64(1);	/* invalidate checksum */

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy = info_original;
	info_copy.major = 2;	/* checksum should be different */

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.sig");
	info_copy = info_original;
	info_copy.sig[0] = 'A';
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.major");
	info_copy = info_original;
	info_copy.major = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.parent_uuid");
	info_copy = info_original;
	memset(info_copy.parent_uuid, 0, BTTINFO_UUID_LEN);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.flags");
	info_copy = info_original;
	info_copy.flags |= htole32(BTTINFO_FLAG_ERROR);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.nfree");
	info_copy = info_original;
	info_copy.nfree = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.external_lbasize");
	info_copy = info_original;
	info_copy.external_lbasize = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy.external_lbasize = htole32(
			le32toh(info_copy.internal_lbasize) + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.internal_nlba");
	info_copy = info_original;
	info_copy.internal_nlba = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.external_nlba");
	info_copy = info_original;
	info_copy.external_nlba = htole32(
			le32toh(info_copy.internal_nlba) + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy.external_nlba = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.infoof");
	info_copy = info_original;
	info_copy.infooff = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy.infooff = htole64(stbuf.st_size + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.flogoff");
	info_copy = info_original;
	info_copy.flogoff = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy.flogoff = htole64(stbuf.st_size + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.mapoff");
	info_copy = info_original;
	info_copy.mapoff = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	info_copy.mapoff = htole64(stbuf.st_size + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.nextoff");
	info_copy = info_original;
	info_copy.nextoff = htole64(stbuf.st_size + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.infosize");
	info_copy = info_original;
	info_copy.infosize = 0;
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), info_off);

	/* verify the consistency of the data */
	check_consistency();

	/* revert original btt_info */
	OUT("Revert btt_info");
	mapped_write(fns, &info_original, sizeof (info_original), info_off);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Invalidate map entry");
	/* set one invalid map entry and mask error and zero bits */
	off_t map_enty_offset = info_off + info_original.mapoff;
	uint32_t invalid_lba = htole32(~0 & BTT_MAP_ENTRY_LBA_MASK);
	/* load the previous map entry */
	uint32_t prev_lba;
	mapped_read(fns, &prev_lba, sizeof (prev_lba), map_enty_offset);

	mapped_write(fns, &invalid_lba, sizeof (invalid_lba), map_enty_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* revert previous value */
	OUT("Revert map entry");
	mapped_write(fns, &prev_lba, sizeof (prev_lba), map_enty_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* set invalid flog pair lba */
	OUT("Invalidate flog entry");
	off_t flog_offset = info_off + info_original.flogoff;
	struct btt_flog flogs[2];

	/* read the first flog pair */
	mapped_read(fns, &flogs, sizeof (struct btt_flog) * 2, flog_offset);

	struct btt_flog flog_original[2];
	memcpy(flog_original, flogs, sizeof (struct btt_flog) * 2);

	/* modify the flog entries */
	flogs[0].lba = invalid_lba;
	flogs[1].lba = invalid_lba;

	/* write the modified flog */
	mapped_write(fns, &flogs, sizeof (struct btt_flog) * 2, flog_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* revert previous value */
	OUT("Revert flog entry");
	mapped_write(fns, &flog_original, sizeof (struct btt_flog) * 2,
			flog_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* modify a few fields in the btt_info backup */
	off_t backup_offset = info_off + info_original.infooff;

	OUT("Change backup btt_info.nextoff");
	info_copy = info_original;
	info_copy.nextoff = htole64(stbuf.st_size + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), backup_offset);

	/* verify the consistency of the data */
	check_consistency();

	OUT("Change btt_info.external_nlba");
	info_copy = info_original;
	info_copy.external_nlba = htole32(
			le32toh(info_copy.internal_nlba) + 1);
	/* correct the checksum */
	util_checksum(&info_copy, sizeof (info_copy), &info_copy.checksum, 1);

	mapped_write(fns, &info_copy, sizeof (info_copy), backup_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* revert original btt_info */
	OUT("Revert backup btt_info");
	mapped_write(fns, &info_original, sizeof (info_original),
			backup_offset);

	/* verify the consistency of the data */
	check_consistency();

	/* change the blk header data and verify consistency */

	struct pool_hdr header_original;	/* original pool header */
	off_t header_off = 0;	/* offset of the pool header */

	/* read the btt info block */
	mapped_read(fns, &header_original, sizeof (header_original),
			header_off);

	/* modifiable copy of the pool header */
	struct pool_hdr header_copy = header_original;

	/* modify fields of the pool header */

	OUT("Change pool_hdr.checksum");
	header_copy.checksum = htole64(1); /* invalidate checksum */

	mapped_write(fns, &header_copy, sizeof (header_copy), header_off);

	/* verify the consistency of the data */
	check_consistency();

	header_copy = header_original;
	header_copy.major = 0;

	mapped_write(fns, &header_copy, sizeof (header_copy), header_off);

	/* verify the consistency of the data */
	check_consistency();

	header_copy = header_original;

	OUT("Change pool_hdr.major");
	header_copy.major = 0;
	/* correct the checksum */
	util_checksum(&header_copy, sizeof (header_copy), &header_copy.checksum,
			1);

	mapped_write(fns, &header_copy, sizeof (header_copy), header_off);

	/* verify the consistency of the data */
	check_consistency();

	MUNMAP(fns, stbuf.st_size);
	CLOSE(fd);

	DONE(NULL);
}
