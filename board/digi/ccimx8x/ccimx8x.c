/*
 * Copyright (C) 2018-2019 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <mmc.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx/cpu.h>
#include <asm/mach-imx/sci/sci.h>
#include <asm/mach-imx/boot_mode.h>

#include "../common/hwid.h"
#include "../common/mca.h"

DECLARE_GLOBAL_DATA_PTR;

struct ccimx8_variant ccimx8x_variants[] = {
/* 0x00 */ { IMX8_NONE,	0, 0, "Unknown"},
/* 0x01 - 55001984-01 */
	{
		IMX8QXP,
		SZ_1G,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Automotive QuadXPlus 1.2GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x02 - 55001984-02 */
	{
		IMX8QXP,
		SZ_2G,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Industrial QuadXPlus 1.0GHz, 16GB eMMC, 2GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x03 - 55001984-03 */
	{
		IMX8QXP,
		SZ_2G,
		0,
		"Industrial QuadXPlus 1.0GHz, 8GB eMMC, 2GB LPDDR4, -40/+85C",
	},
/* 0x04 - 55001984-04 */
	{
		IMX8DX,
		SZ_1G,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Industrial DualX 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x05 - 55001984-05 */
	{
		IMX8DX,
		SZ_1G,
		0,
		"Industrial DualX 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C",
	},
/* 0x06 - 55001984-06 */
	{
		IMX8DX,
		SZ_512M,
		0,
		"Industrial DualX 1.0GHz, 8GB eMMC, 512MB LPDDR4, -40/+85C",
	},
/* 0x07 - 55001984-07 */
	{
		IMX8QXP,
		SZ_1G,
		0,
		"Industrial QuadXPlus 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C",
	},
/* 0x08 - 55001984-08 */
	{
		IMX8QXP,
		SZ_1G,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Industrial QuadXPlus 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x09 - 55001984-09 */
	{
		IMX8DX,
		SZ_512M,
		0,
		"Industrial DualX 1.0GHz, 8GB eMMC, 512MB LPDDR4, -40/+85C",
	},
/* 0x0A - 55001984-10 */
	{
		IMX8DX,
		SZ_1G,
		0,
		"Industrial DualX 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C",
	},
/* 0x0B - 55001984-11 */
	{
		IMX8DX,
		SZ_1G,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Industrial DualX 1.0GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
};

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC1_BOOT:
		return 0;	/* index of USDHC1 (eMMC) */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

uint mmc_get_env_part(struct mmc *mmc)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 0;	/* When booting from an SD card the
				 * environment will be saved to the unique
				 * hardware partition: 0 */
	case MMC1_BOOT:
	default:
		return CONFIG_SYS_MMC_ENV_PART;
				/* When booting from USDHC1 (eMMC) the
				 * environment will be saved to boot
				 * partition 2 to protect it from
				 * accidental overwrite during U-Boot update */
	}
}

int hwid_in_db(int variant)
{
	if (variant < ARRAY_SIZE(ccimx8x_variants))
		if (ccimx8x_variants[variant].cpu != IMX8_NONE)
			return 1;

	return 0;
}

int board_lock_hwid(void)
{
	/* SCU performs automatic lock after programming */
	printf("not supported. Fuses automatically locked after programming.\n");
	return 1;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
	puts("SCI reboot request");
	sc_pm_reboot(SC_IPC_CH, SC_PM_RESET_TYPE_COLD);
	while (1)
		putc('.');
}

void detail_board_ddr_info(void)
{
	puts("\nDDR    ");
}

void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     disk_partition_t *info)
{
	/* Use a different offset depending on the i.MX8X QXP CPU revision */
	u32 cpurev = get_cpu_rev();
	struct mmc *mmc = find_mmc_device(EMMC_BOOT_DEV);
	int part = (mmc->part_config >> 3) & PART_ACCESS_MASK;

	switch (cpurev & 0xFFF) {
	case CHIP_REV_A:
		info->start = EMMC_BOOT_PART_OFFSET_A0 / mmc_dev->blksz;
		break;
	case CHIP_REV_B:
		info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
		break;
	default:
		/*
		 * Starting from RevC, use a different offset depending on the
		 * target device and partition:
		 * - For eMMC BOOT1 and BOOT2
		 *	Offset = 0
		 * - For eMMC User Data area.
		 *	Offset = EMMC_BOOT_PART_OFFSET
		 */
		if (part == 1 || part == 2) {
			/* eMMC BOOT1 or BOOT2 partitions */
			info->start = 0;
		} else {
			info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
		}
		break;
	}
	/* Boot partition size - Start of boot image */
	info->size = (mmc->capacity_boot / mmc_dev->blksz) - info->start;
}
