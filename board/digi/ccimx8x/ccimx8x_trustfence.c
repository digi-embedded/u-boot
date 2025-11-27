/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <command.h>
#include <common.h>
#include <fuse.h>
#include <part.h>
#include <stdlib.h>
#include <asm/mach-imx/ahab.h>
#include <firmware/imx/sci/sci.h>

#include "../common/trustfence.h"

int confirm_close(void);
extern void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     struct disk_partition *info);
extern int mmc_get_bootdevindex(void);

bool trustfence_is_closed(void)
{
	sc_err_t err;
	uint16_t lc;

	err = sc_seco_chip_info(-1, &lc, NULL, NULL, NULL);
	if (err != SC_ERR_NONE) {
		printf("Error in get lifecycle\n");
		return true;
	}

	return ((lc & 0x80) == 0x80);
}

int close_device(int confirmed_close)
{
	int err;
	uint16_t lc;
	uint8_t idx = 0U;

	if (!confirmed_close && !confirm_close())
		return -EACCES;

	err = sc_seco_chip_info(-1, &lc, NULL, NULL, NULL);
	if (err != SC_ERR_NONE) {
		printf("Error in get lifecycle\n");
		return -EIO;
	}

	if (lc != 0x20) {
		printf("Current lifecycle is NOT NXP closed, can't move to OEM closed (0x%x)\n", lc);
		return -EPERM;
	}

	/* Verifiy that we do not have any AHAB events */
	err = sc_seco_get_event(-1, idx, NULL);
	if (err == SC_ERR_NONE) {
		printf ("SECO Event[%u] - abort closing device.\n", idx);
		return -EIO;
	}

	err = sc_seco_forward_lifecycle(-1, 16);
	if (err != SC_ERR_NONE) {
		printf("Error in forward lifecycle to OEM closed\n");
		return -EIO;
	}

	puts("Closing device...\n");

	return 0;
}

int revoke_keys(void)
{
	int err;

	err = seco_commit(-1, 0x10);
	if (err != SC_ERR_NONE) {
		printf("%s: Error in seco_commit\n", __func__);
		return -EIO;
	}

	return 0;
}

/* Trustfence helper functions */
int trustfence_status(void)
{
	uint8_t idx = 0U;
	int err;

	if (!is_usb_boot())
		printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted() ?
				"[YES]" : "[NO]");
	puts("* AHAB events:\t\t");
	err = sc_seco_get_event(-1, idx, NULL);
	if (err == SC_ERR_NONE)
		idx++;
	if (idx == 0)
		puts("[NO ERRORS]\n");
	else
		puts("[ERRORS PRESENT!]\n");

	return err;
}

void board_print_trustfence_jtag_mode(u32 *sjc)
{
	return;
}

void board_print_trustfence_jtag_key(u32 *sjc)
{
	return;
}

int sense_key_status(u32 *val)
{
	if (fuse_sense(CONFIG_TRUSTFENCE_SRK_REVOKE_BANK,
			CONFIG_TRUSTFENCE_SRK_REVOKE_WORD,
			val))
		return -1;

	*val = (*val >> CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET) &
		CONFIG_TRUSTFENCE_SRK_REVOKE_MASK;

	return 0;
}

/* Read the SRK Revoke mask from the Container header */
int get_srk_revoke_mask(u32 *mask)
{
	int ret = CMD_RET_SUCCESS;
	int mmc_dev_index, mmc_part;
	struct disk_partition info;
	struct blk_desc *mmc_dev;
	uint blk_cnt, blk_start;
	char *buffer = NULL;
	struct container_hdr *second_cont;
	u32 buffer_size = 0;
	u16 ctnr_hdr_align = container_hdr_alignment();

	/* Container Header can only be read from the storage media */
	if (is_usb_boot())
		return CMD_RET_FAILURE;

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

	/* Read mask from the Second Container Header Flags (11:8) */
	second_cont = (struct container_hdr *)(buffer+ctnr_hdr_align);
	*mask = (second_cont->flags>>8) & 0xF;

sanitize:
	/* Sanitize memory */
	memset(buffer, '\0', sizeof(buffer));
	free(buffer);
	buffer = NULL;

	return ret;
}
