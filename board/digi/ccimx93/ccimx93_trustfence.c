/*
 * Copyright 2023 Digi International Inc
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <command.h>
#include <common.h>
#include <malloc.h>
#include <mmc.h>
#include <asm/io.h>
#include <asm/mach-imx/ele_api.h>
#include <asm/mach-imx/image.h>

#include "../common/trustfence.h"

/* see arch/arm/mach-imx/ele_ahab.c */
#define AHAB_MAX_EVENTS 8
int confirm_close(void);

/*
 * Trustfence helper functions
 */

int close_device(int confirmed_close)
{
	int err;
	u32 lc;
	u32 events[AHAB_MAX_EVENTS];
	u32 cnt = AHAB_MAX_EVENTS;

	if (!confirmed_close && !confirm_close())
		return -EACCES;

	lc = readl(FSB_BASE_ADDR + 0x41c);
	lc &= 0x3ff;

	if (lc != 0x8) {
		printf
		    ("Current lifecycle is NOT OEM open, can't move to OEM closed (0x%x)\n",
		     lc);
		return -EPERM;
	}

	err = ahab_get_events(events, &cnt, NULL);
	if (err || cnt) {
		printf("AHAB events found!\n");
		return -EIO;
	}

	err = ahab_forward_lifecycle(8, NULL);
	if (err != 0) {
		printf("Error in forward lifecycle to OEM closed\n");
		return -EIO;
	}
	printf("Device closed successfully\n");

	return 0;
}

bool imx_hab_is_enabled(void)
{
	uint32_t lc;

	lc = readl(FSB_BASE_ADDR + 0x41c);
	lc &= 0x3f;

	if (lc != 0x20)
		return false;
	else
		return true;
}

int trustfence_status(void)
{
	u32 events[AHAB_MAX_EVENTS];
	u32 cnt = AHAB_MAX_EVENTS;
	int ret;

	printf("* Encrypted U-Boot:\t%s\n", is_uboot_encrypted()?
	       "[YES]" : "[NO]");
	puts("* AHAB events:\t\t");
	ret = ahab_get_events(events, &cnt, NULL);
	if (ret)
		puts("[GET EVENTS FAILURE]\n");
	else if (!cnt)
		puts("[NO ERRORS]\n");
	else
		puts("[ERRORS PRESENT!]\n");

	return 0;
}

extern int mmc_get_bootdevindex(void);

/* Read the SRK Revoke mask from the Container header*/
int get_srk_revoke_mask(u32 *mask)
{
	struct container_hdr *hdr;
	struct mmc *mmc;
	u32 buf_size, offset;
	u8 *buf, part;
	int ret = CMD_RET_SUCCESS;

	/* Container Header can only be read from the storage media */
	if (is_usb_boot()) {
		printf
		    ("Reading revoke mask is not supported booting from USB.\n");
		return CMD_RET_FAILURE;
	}

	mmc = find_mmc_device(mmc_get_bootdevindex());
	if (!mmc) {
		printf("Failed to get mmc device.\n");
		return CMD_RET_FAILURE;
	}

	offset = CONTAINER_HDR_EMMC_OFFSET;
	part = EXT_CSD_EXTRACT_BOOT_PART(mmc->part_config);
	/* Reset if booting from SD */
	if (part == 7) {
		offset = CONTAINER_HDR_MMCSD_OFFSET;
		part = 0;
	}

	ret = blk_select_hwpart(mmc_get_blk_desc(mmc)->bdev, part);
	if (ret) {
		printf("Failed to switch to boot partition %d.\n", part);
		return CMD_RET_FAILURE;
	}

	/* Add 80 bytes to make room for DEK blob if there is one */
	buf_size = roundup(CONTAINER_HDR_ALIGNMENT + 80, mmc->read_bl_len);
	buf = malloc(buf_size);
	if (!buf)
		return CMD_RET_FAILURE;

	if (!blk_dread
	    (mmc_get_blk_desc(mmc), offset / mmc->read_bl_len,
	     buf_size / mmc->read_bl_len, buf)) {
		printf("Read container image from MMC/SD failed.\n");
		ret = CMD_RET_FAILURE;
		goto sanitize;
	}

	hdr = (struct container_hdr *)(buf + CONTAINER_HDR_ALIGNMENT);
	if (hdr->tag != 0x87 || hdr->version != 0x0) {
		printf("Error: wrong container header.\n");
		return CMD_RET_FAILURE;
	}
	*mask = (hdr->flags >> 8) & 0xf;

sanitize:
	memset(buf, 0, buf_size);
	free(buf);
	buf = NULL;

	return ret;
}

int revoke_keys(void)
{
	u32 index = 0x10 /* SRK revocation of the OEM container */ ;
	u32 info_type;
	int ret = -1;

	if (ahab_commit(index, NULL, &info_type))
		goto err;

	ret = (index != info_type);
	if (ret)
		goto err;

err:
	return ret ? -1 : 0;
}

/*
 * TODO: so far NXP has not provided the TRUSTFENCE_SRK_REVOKE fuse
 * information, so read the mask again from the container.
 */
int sense_key_status(u32 *val)
{
	int ret = -1;

	/* Container Header can only be read from the storage media */
	if (is_usb_boot())
		printf
		    ("Reading revoke mask is not supported booting from USB.\n");
	else
		ret = get_srk_revoke_mask(val);

	return ret ? -1 : 0;
}
