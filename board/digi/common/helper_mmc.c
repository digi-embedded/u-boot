/*
 *  Copyright (C) 2017 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <common.h>
#ifdef CONFIG_FSL_ESDHC
#include <fsl_esdhc.h>
#endif
#include <environment.h>
#include <mmc.h>
#include <malloc.h>
#include <otf_update.h>

extern int mmc_get_bootdevindex(void);

/*
 * Common function to get the block size of the storage media.
 */
size_t media_get_block_size(void)
{
	size_t block_size = 0;
	struct mmc *mmc;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (mmc)
		block_size = mmc->block_dev.blksz;
	return block_size;
}

/*
 * Get the offset of a partition in the mmc
 * @in: Partition name
 * @in: Offset of the partition in mmc block units
 * return 0 if success
*/
int get_partition_offset(char *part_name, lbaint_t *offset)
{
	disk_partition_t info;
	char dev_index_str[2];
	int r;

	sprintf(dev_index_str, "%d", mmc_get_bootdevindex());
	r = get_partition_bynameorindex(CONFIG_SYS_STORAGE_MEDIA,
					dev_index_str,
					part_name,
					&info);
	if (r < 0)
		return r;
	*offset = info.start;
	return 0;
}

/*
 * Get the TF Key offset(in blocks) in the U-Boot environment
 */
unsigned int get_filesystem_key_offset(void)
{
	return (CONFIG_ENV_OFFSET_REDUND + CONFIG_ENV_SIZE_REDUND) /
		media_get_block_size();
}

/*
 * Read a data block from the storage media.
 * This function only reads one mmc block
 * @in: Address in media (must be aligned to block size)
 * @in: Read buffer
 * @in: Partition index, only applies for MMC
 * The function returns:
 *	0 if success and data in readbuf.
 *	-1 on error
 */
int media_read_block(uintptr_t addr, unsigned char *readbuf, uint hwpart)
{
	size_t len;
	size_t nbytes;
	int ret = -1;
	struct mmc *mmc;
	uint orig_part;
	void *loadaddr = (void *) getenv_ulong("loadaddr", 16, load_addr);

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc)
		return ret;

	orig_part = mmc->block_dev.hwpart;
	if (blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				     hwpart))
		return ret;

	nbytes = mmc->block_dev.block_read(&mmc->block_dev,
					   addr,
					   1,
					   loadaddr);
	if (nbytes == 1) {
		memcpy(readbuf, loadaddr, len);
		ret = 0;
	}
	blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				 orig_part);
	return ret;
}

/*
 * Write a data block to the storage media.
 * This function only writes the minimum required amount of data:
 *	for mmc: one block (probably 512 bytes)
 * @in: Address in media (must be aligned to block size)
 * @in: Write buffer
 * @in: Partition index, only applies for MMC
 * The function returns:
 *	0 if success
 *	-1 on error
 */
int media_write_block(uintptr_t addr, unsigned char *writebuf, uint hwpart)
{
	size_t len;
	int ret = -1;
	struct mmc *mmc;
	static struct blk_desc *mmc_dev;
	unsigned long written;
	uint orig_part;

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc)
		return ret;

	mmc_dev = mmc_get_blk_desc(mmc);
	orig_part = mmc->block_dev.hwpart;
	if (blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				     hwpart))
		return ret;
	written = mmc->block_dev.block_write(mmc_dev,
					     addr,
					     1,
					     writebuf);
	if (written == 1)
		ret = 0;
	blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				 orig_part);
	return ret;
}

/*
 * Erase data in the storage media.
 * This function only erases the minimum required amount of data:
 *	for mmc: one block (probably 512 bytes)
 * @in: Address in media (must be aligned to block size)
 * @in: Partition index, only applies for MMC
 */
void media_erase_fskey(uintptr_t addr, uint hwpart)
{
	struct mmc *mmc;
	unsigned char *zero_buf;
	uint orig_part = 0;

	zero_buf = calloc(1, media_get_block_size());
	if (!zero_buf)
		return;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc)
		return;
	orig_part = mmc->block_dev.hwpart;
	if (blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				     hwpart))
		return;
	/*
	 * Do not use block_erase, it uses different erase ranges
	 *  and will erase the full environment.
	 */
	media_write_block(addr, zero_buf, mmc_get_env_part(mmc));
	blk_select_hwpart_devnum(IF_TYPE_MMC, CONFIG_SYS_MMC_ENV_DEV,
				 orig_part);
}

uint get_env_hwpart(void)
{
	struct mmc *mmc;

	mmc = find_mmc_device(mmc_get_bootdevindex());
	return mmc_get_env_part(mmc);
}

#ifdef CONFIG_FSL_ESDHC
extern int mmc_get_bootdevindex(void);

static int write_chunk(struct mmc *mmc, struct blk_desc *mmc_dev,
		       otf_data_t *otfd, lbaint_t dstblk,
		       unsigned int chunklen)
{
	int sectors;
	unsigned long written, read;
	void *verifyaddr;

	printf("\nWriting chunk...");
	/* Check WP */
	if (mmc_getwp(mmc) == 1) {
		printf("[Error]: card is write protected!\n");
		return -1;
	}

	/* We can only write whole sectors (multiples of mmc_dev->blksz bytes)
	 * so we need to check if the chunk is a whole multiple or else add 1
	 */
	sectors = chunklen / mmc_dev->blksz;
	if (chunklen % mmc_dev->blksz)
		sectors++;

	/* Check if chunk fits */
	if (sectors + dstblk > otfd->part->start + otfd->part->size) {
		printf("[Error]: length of data exceeds partition size\n");
		return -1;
	}

	/* Write chunklen bytes of chunk to media */
	debug("writing chunk of 0x%x bytes (0x%x sectors) "
		"from 0x%p to block " LBAFU "\n",
		chunklen, sectors, otfd->loadaddr, dstblk);
	written = mmc->block_dev.block_write(mmc_dev, dstblk, sectors,
					     otfd->loadaddr);
	if (written != sectors) {
		printf("[Error]: written sectors != sectors to write\n");
		return -1;
	}
	printf("[OK]\n");

	/* Verify written chunk if $loadaddr + chunk size does not overlap
	 * $verifyaddr (where the read-back copy will be placed)
	 */
	verifyaddr = (void *) getenv_ulong("verifyaddr", 16, 0);
	if (otfd->loadaddr + sectors * mmc_dev->blksz < verifyaddr) {
		/* Read back data... */
		printf("Reading back chunk...");
		read = mmc->block_dev.block_read(mmc_dev, dstblk, sectors,
						 verifyaddr);
		if (read != sectors) {
			printf("[Error]: read sectors != sectors to read\n");
			return -1;
		}
		printf("[OK]\n");
		/* ...then compare */
		printf("Verifying chunk...");
		if (memcmp(otfd->loadaddr, verifyaddr,
			      sectors * mmc_dev->blksz)) {
			printf("[Error]\n");
			return -1;
		} else {
			printf("[OK]\n");
			return 0;
		}
	} else {
		printf("[Warning]: Cannot verify chunk. "
			"It overlaps $verifyaddr!\n");
		return 0;
	}
}

/* writes a chunk of data from RAM to main storage media (eMMC) */
int update_chunk(otf_data_t *otfd)
{
	static unsigned int chunk_len = 0;
	static lbaint_t dstblk = 0;
	static struct blk_desc *mmc_dev = NULL;
	static int mmc_dev_index = -1;
	static struct mmc *mmc = NULL;

	if (mmc_dev_index == -1) {
		mmc_dev_index = getenv_ulong("mmcdev", 16,
					     mmc_get_bootdevindex());
		mmc = find_mmc_device(mmc_dev_index);
		if (NULL == mmc) {
			printf("ERROR: failed to get MMC device\n");
			return -1;
		}
		mmc_dev = mmc_get_blk_desc(mmc);
		if (NULL == mmc_dev) {
			printf("ERROR: failed to get block descriptor for MMC device\n");
			return -1;
		}
	}

	/*
	 * There are two variants:
	 *  - otfd.buf == NULL
	 *  	In this case, the data is already waiting on the correct
	 *  	address in RAM, waiting to be written to the media.
	 *  - otfd.buf != NULL
	 *  	In this case, the data is on the buffer and must still be
	 *  	copied to an address in RAM, before it is written to media.
	 */
	if (otfd->buf && otfd->len) {
		/*
		 * If data is in the otfd->buf buffer, copy it to the loadaddr
		 * in RAM until we have a chunk that is at least as large as
		 * CONFIG_OTF_CHUNK, to write it to media.
		 */
		memcpy(otfd->loadaddr + otfd->offset, otfd->buf, otfd->len);
	}

	/* Initialize dstblk and local variables */
	if (otfd->flags & OTF_FLAG_INIT) {
		chunk_len = 0;
		dstblk = otfd->part->start;
		otfd->flags &= ~OTF_FLAG_INIT;
	}
	chunk_len += otfd->len;

	/*
	 * If the chunk is completed or if it is the last chunk (OTF_FLAG_FLUSH
	 * set) write it to storage.
	 */
	if (chunk_len >= CONFIG_OTF_CHUNK || (otfd->flags & OTF_FLAG_FLUSH)) {
		unsigned int remaining;

		if (otfd->flags & OTF_FLAG_FLUSH) {
			/* Write all pending data (this is the last chunk) */
			remaining = 0;
		} else {
			/* We have CONFIG_OTF_CHUNK (or more) bytes in RAM.
			* Let's proceed to write as many as multiples of blksz
			* as possible.
			*/
			remaining = chunk_len % mmc_dev->blksz;
			chunk_len -= remaining;
			/* chunk_len is now multiple of blksz */
		}

		if (write_chunk(mmc, mmc_dev, otfd, dstblk, chunk_len))
			return -1;

		/* increment destiny block */
		dstblk += (chunk_len / mmc_dev->blksz);
		/* copy excess of bytes from previous chunk to offset 0 */
		if (remaining) {
			memcpy(otfd->loadaddr, otfd->loadaddr + chunk_len,
			       remaining);
			debug("Copying excess of %d bytes to offset 0\n",
			      remaining);
		}
		/* reset chunk_len to excess of bytes from previous chunk
		 * (or zero, if that's the case) */
		chunk_len = remaining;
	}

	/*
	 * This flag signals the last block, so we have finished.
	 * Reset all static variables.
	 */
	if (otfd->flags & OTF_FLAG_FLUSH) {
		chunk_len = 0;
		dstblk = 0;
		mmc_dev = NULL;
		mmc_dev_index = -1;
		mmc = NULL;

		return 0;
	}

	/* Set otfd offset pointer to offset in RAM where new bytes would
	 * be written. This offset may be reused by caller */
	otfd->offset = chunk_len;

	return 0;
}
#endif /* CONFIG_FSL_ESDHC */
