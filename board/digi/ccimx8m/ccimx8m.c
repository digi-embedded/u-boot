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
#include <spl.h>

DECLARE_GLOBAL_DATA_PTR;

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 0;	/* index of USDHC2 (SD card) */
	case MMC3_BOOT:
		return 1;	/* index of USDHC3 (eMMC) */
	default:
		/* return default value otherwise */
		return CONFIG_SYS_MMC_ENV_DEV;
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
		return 2;	/* When booting from USDHC3 (eMMC) the
				 * environment will be saved to boot
				 * partition 2 to protect it from
				 * accidental overwrite during U-Boot update */
	default:
		return CONFIG_SYS_MMC_ENV_PART;
	}
}

int dram_init(void)
{
	/* rom_pointer[1] contains the size of TEE occupies */
	if (rom_pointer[1])
		gd->ram_size = PHYS_SDRAM_SIZE - rom_pointer[1];
	else
		gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}

void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     disk_partition_t *info)
{
	struct mmc *mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
	int part = (mmc->part_config >> 3) & PART_ACCESS_MASK;

	/*
	 * Use a different offset depending on the target device and partition:
	 * - For eMMC BOOT1 and BOOT2
	 *	Offset = 0
	 * - For eMMC User Data area.
	 *	Offset = CONFIG_SYS_BOOT_PART_OFFSET
	 */
	if (part == 1 || part == 2) {
		/* eMMC BOOT1 or BOOT2 partitions */
		info->start = 0;
	} else {
		info->start = CONFIG_SYS_BOOT_PART_OFFSET / mmc_dev->blksz;
	}
	info->size = CONFIG_SYS_BOOT_PART_SIZE / mmc_dev->blksz;
}
