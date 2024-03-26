/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <command.h>
#include <errno.h>
#include <linux/types.h>
#include <log.h>
#include <malloc.h>
#include <mmc.h>
#include <string.h>

#define AHAB_AUTH_CONTAINER_TAG		0x87
#define AHAB_AUTH_BLOB_TAG		0x81
#define AHAB_VERSION			0x00

extern int mmc_get_bootdevindex(void);
extern void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
					    struct disk_partition *info);

int get_dek_blob_offset(char *address, u32 *offset)
{
	char *nd_cont_header;
	char *nd_sig_block;
	char *dek_blob;
	int cont_header_offset = 0x400;
	int sig_block_offset;
	int blob_offset;

	/* Second Container Header is set with a 1KB padding */
	nd_cont_header = address + cont_header_offset;
	debug("Second Container Header Address: 0x%lx\n",
	      (long unsigned int)nd_cont_header);

	if (address[0] != AHAB_VERSION || address[3] != AHAB_AUTH_CONTAINER_TAG) {
		debug("Tag does not match as expected\n");
		return -EINVAL;
	}

	sig_block_offset =
	    (((nd_cont_header[13] & 0xff) << 8) | (nd_cont_header[12] & 0xff));
	debug("Signature Block offset is 0x%04x \n", sig_block_offset);
	nd_sig_block = nd_cont_header + sig_block_offset;
	debug("Second Signature Block Address: 0x%lx\n",
	      (long unsigned int)nd_sig_block);

	blob_offset =
	    (((nd_sig_block[11] & 0xff) << 8) | (nd_sig_block[10] & 0xff));
	debug("DEK Blob offset is 0x%04x \n", blob_offset);
	dek_blob = nd_sig_block + blob_offset;
	debug("DEK Blob Address: 0x%lx\n", (long unsigned int)dek_blob);
	*offset = dek_blob - address;

	return 0;
}

int get_dek_blob_size(char *address, u32 *size)
{
	if (address[0] != AHAB_VERSION || address[3] != AHAB_AUTH_BLOB_TAG) {
		debug("Tag does not match as expected\n");
		return -EINVAL;
	}

	*size = (((address[2] & 0xff) << 8) | (address[1] & 0xff));
	debug("DEK blob size is 0x%04x\n", *size);

	return 0;
}

int get_dek_blob(char *output, u32 *size)
{
	int ret, mmc_dev_index, mmc_part;
	struct disk_partition info;
	struct blk_desc *mmc_dev;
	uint blk_cnt, blk_start;
	char *buffer = NULL;
	char *dek_blob_src = NULL;
	u32 buffer_size = 0;
	u32 dek_blob_size = 0;
	u32 dek_blob_offset = 0;

	/* Obtain storage media settings */
	mmc_dev_index = env_get_ulong("mmcbootdev", 0, mmc_get_bootdevindex());
	if (mmc_dev_index == EMMC_BOOT_DEV) {
		mmc_part = env_get_ulong("mmcbootpart", 0, EMMC_BOOT_PART);
	} else {
		/*
		 * When booting from an SD card there is
		 * a unique hardware partition: 0
		 */
		mmc_part = 0;
	}
	mmc_dev = blk_get_devnum_by_uclass_id(UCLASS_MMC, mmc_dev_index);
	if (NULL == mmc_dev) {
		debug("Cannot determine sys storage device\n");
		return CMD_RET_FAILURE;
	}
	calculate_uboot_update_settings(mmc_dev, &info);
	blk_start = info.start;
	/* Second Container Header is set with a 1KB padding + 3KB Header info */
	buffer_size = SZ_4K;
	blk_cnt = buffer_size / mmc_dev->blksz;

	/* Initialize boot partition */
	ret = blk_select_hwpart_devnum(UCLASS_MMC, mmc_dev_index, mmc_part);
	if (ret != 0) {
		debug("Error to switch to partition %d on dev %d (%d)\n",
		      mmc_part, mmc_dev_index, ret);
		return CMD_RET_FAILURE;
	}

	/* Read from boot media */
	buffer = malloc(roundup(buffer_size, mmc_dev->blksz));
	if (!buffer)
		return -ENOMEM;
	debug("MMC read: dev # %u, block # %u, count %u ...\n",
	      mmc_dev_index, blk_start, blk_cnt);
	if (!blk_dread(mmc_dev, blk_start, blk_cnt, buffer)) {
		ret = CMD_RET_FAILURE;
		goto sanitize;
	}

	/* Recover DEK blob placed into the Signature Block */
	debug("Recovering DEK blob...\n");
	ret = get_dek_blob_offset(buffer, &dek_blob_offset);
	if (ret != 0) {
		debug("Error getting the DEK Blob offset (%d)\n", ret);
		goto sanitize;
	}

	/* Obtain the DEK Blob size */
	dek_blob_src = buffer + dek_blob_offset;
	ret = get_dek_blob_size(dek_blob_src, &dek_blob_size);
	if (ret != 0) {
		debug("Error getting the DEK Blob size (%d)\n", ret);
		goto sanitize;
	}

	/* Save DEK Blob */
	memcpy(output, dek_blob_src, dek_blob_size);

sanitize:
	/* Sanitize memory */
	memset(buffer, '\0', sizeof(buffer));
	free(buffer);
	buffer = NULL;

	return ret;
}
