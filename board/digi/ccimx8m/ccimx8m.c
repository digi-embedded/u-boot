/*
 * Copyright 2019 Digi International Inc
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <mmc.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/hab.h>
#include <spl.h>

#include "../common/hwid.h"

DECLARE_GLOBAL_DATA_PTR;

#define SPL_IVT_HEADER_SIZE 0x40
/* The Blob Address is a fixed address defined in imx-mkimage
 * project in iMX8M/soc.mak file
 */
#define DEK_BLOB_LOAD_ADDR 0x40400000

#define HAB_AUTH_BLOB_TAG              0x81
#define HAB_VERSION                    0x43

int board_phys_sdram_size(phys_size_t *size)
{
	/* Default to RAM size of DVK variant 0x01 (1 GiB) */
	u64 ram;
	struct digi_hwid my_hwid;

	/* Default to minimum RAM size for each platform */
	if (is_imx8mn())
		ram = SZ_512M;  /* ccimx8mn variant 0x03 (512MB) */
	else
		ram = SZ_1G;    /* ccimx8mm variant 0x01 (1GB) */

	if (board_read_hwid(&my_hwid)) {
		debug("Cannot read HWID. Using default DDR configuration.\n");
		my_hwid.ram = 0;
	}

	if (my_hwid.ram)
		ram = hwid_get_ramsize(&my_hwid);

	*size = ram;

	return 0;
}

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC3_BOOT:
		return 0;	/* index of USDHC3 (eMMC) */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

uint mmc_get_env_part(struct mmc *mmc)
{
	switch(get_boot_device()) {
	case SD1_BOOT ... SD3_BOOT:
		return 0;	/* When booting from an SD card the
				 * environment will be saved to the unique
				 * hardware partition: 0 */
	case MMC3_BOOT:
	default:
		return CONFIG_SYS_MMC_ENV_PART;
				/* When booting from USDHC3 (eMMC) the
				 * environment will be saved to boot
				 * partition 2 to protect it from
				 * accidental overwrite during U-Boot update */
	}
}

void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     disk_partition_t *info)
{
	struct mmc *mmc = find_mmc_device(EMMC_BOOT_DEV);
	int part = env_get_ulong("mmcbootpart", 10, EMMC_BOOT_PART);

	/*
	 * Use a different offset depending on the target device and partition:
	 * - For eMMC BOOT1 and BOOT2
	 *	Offset = 0
	 * - For eMMC User Data area.
	 *	Offset = EMMC_BOOT_PART_OFFSET
	 */
	if (is_imx8mn() && (part == 1 || part == 2)) {
		/* eMMC BOOT1 or BOOT2 partitions */
		info->start = 0;
	} else {
		info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
	}
	/* Boot partition size - Start of boot image */
	info->size = (mmc->capacity_boot / mmc_dev->blksz) - info->start;
}

int get_dek_blob_offset(char *address, u32 *offset)
{
	struct ivt *ivt = (struct ivt *)(CONFIG_SPL_TEXT_BASE - SPL_IVT_HEADER_SIZE);

	/* Verify the pointer is pointing at an actual IVT table */
	if ((ivt->hdr.magic != IVT_HEADER_MAGIC) ||
	   (be16_to_cpu(ivt->hdr.length) != IVT_TOTAL_LENGTH))
		return 1;

	if (ivt->csf)
		*offset = ivt->csf - (CONFIG_SPL_TEXT_BASE - SPL_IVT_HEADER_SIZE) + CONFIG_CSF_SIZE;
	else
		return 1;

	return 0;
}

int get_dek_blob_size(char *address, u32 *size)
{
	if (address[3] != HAB_VERSION || address[0] != HAB_AUTH_BLOB_TAG) {
		debug("Tag does not match as expected\n");
		return -EINVAL;
	}

	*size = address[2];
	debug("DEK blob size is 0x%04x\n", *size);

	return 0;
}

int get_dek_blob(char *output, u32 *size)
{
	/* Get DEK offset */
	char *dek_blob_src = (void*)(DEK_BLOB_LOAD_ADDR);
	u32 dek_blob_size;

	/* Get Dek blob */
	if (get_dek_blob_size((char *)dek_blob_src, &dek_blob_size))
		return 1;

	memcpy(output, dek_blob_src, dek_blob_size);
	*size = dek_blob_size;

	return 0;
}
