/*
 *  Copyright (C) 2017 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/
#include <common.h>
#include <mmc.h>
#include <environment.h>
#include <malloc.h>

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
int get_partition_offset(char *part_name, u32 *offset)
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
int media_read_block(u32 addr, unsigned char *readbuf, uint hwpart)
{
	size_t len;
	size_t nbytes;
	int ret = -1;
	struct mmc *mmc;
	uint orig_part;
	u32 loadaddr = getenv_ulong("loadaddr", 16, load_addr);

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc)
		return ret;

	orig_part = mmc->part_num;
	if (mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, hwpart))
		return ret;

	nbytes = mmc->block_dev.block_read(CONFIG_SYS_MMC_ENV_DEV,
					   addr,
					   1,
					   (void *)loadaddr);
	if (nbytes == 1) {
		memcpy(readbuf, (const void *)loadaddr, len);
		ret = 0;
	}
	mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, orig_part);
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
int media_write_block(u32 addr, unsigned char *writebuf, uint hwpart)
{
	size_t len;
	int ret = -1;
	struct mmc *mmc;
	unsigned long written;
	uint orig_part;

	len = media_get_block_size();
	if (len <= 0)
		return ret;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc)
		return ret;

	orig_part = mmc->part_num;
	if (mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, hwpart))
		return ret;
	written = mmc->block_dev.block_write(CONFIG_SYS_MMC_ENV_DEV,
					     addr,
					     1,
					     writebuf);
	if (written == 1)
		ret = 0;
	mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, orig_part);
	return ret;
}

/*
 * Erase data in the storage media.
 * This function only erases the minimum required amount of data:
 *	for mmc: one block (probably 512 bytes)
 * @in: Address in media (must be aligned to block size)
 * @in: Partition index, only applies for MMC
 */
void media_erase_fskey(u32 addr, uint hwpart)
{
	struct mmc *mmc;
	unsigned char *zero_buf;
	uint orig_part = 0;

	zero_buf = calloc(1, media_get_block_size());
	if (!zero_buf)
		return;

	mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	if (!mmc) {
		free(zero_buf);
		return;
	}
	orig_part = mmc->part_num;
	if (mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, hwpart)) {
		free(zero_buf);
		return;
	}
	/*
	 * Do not use block_erase, it uses different erase ranges
	 *  and will erase the full environment.
	 */
	media_write_block(addr, zero_buf, mmc_get_env_part(mmc));
	mmc_switch_part(CONFIG_SYS_MMC_ENV_DEV, orig_part);
}

uint get_env_hwpart(void)
{
	struct mmc *mmc;

	mmc = find_mmc_device(mmc_get_bootdevindex());
	return mmc_get_env_part(mmc);
}
