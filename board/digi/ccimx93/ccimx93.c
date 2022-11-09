/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/mach-imx/boot_mode.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <mmc.h>

enum env_location env_get_location(enum env_operation op, int prio)
{
	enum env_location env_loc = ENVL_UNKNOWN;

	if (prio)
		return env_loc;

	if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
		env_loc = ENVL_MMC;
	else if (CONFIG_IS_ENABLED(ENV_IS_NOWHERE))
		env_loc = ENVL_NOWHERE;

	return env_loc;
}

int mmc_get_bootdevindex(void)
{
	switch (get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC1_BOOT:
		return 0;	/* index of USDHC1 (eMMC) */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

#ifdef CONFIG_FSL_ESDHC_IMX
int board_mmc_get_env_dev(int devno)
{
	return mmc_get_bootdevindex();
}

bool board_has_emmc(void)
{
	return 1;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     struct disk_partition *info)
{
	struct mmc *mmc = find_mmc_device(EMMC_BOOT_DEV);
	int part = env_get_ulong("mmcbootpart", 10, EMMC_BOOT_PART);

	/*
	 * Use a different offset depending on the target device and partition:
	 * - For eMMC BOOT1 and BOOT2
	 *      Offset = 0
	 * - For eMMC User Data area.
	 *      Offset = EMMC_BOOT_PART_OFFSET
	 */
	if (is_imx93() && (part == 1 || part == 2)) {
		/* eMMC BOOT1 or BOOT2 partitions */
		info->start = 0;
	} else {
		info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
	}

	/* Boot partition size - Start of boot image */
	info->size = (mmc->capacity_boot / mmc_dev->blksz) - info->start;
}

void som_default_environment(void)
{
#ifdef CONFIG_CMD_MMC
	char cmd[80];

	/* Set $mmcbootdev to MMC boot device index */
	sprintf(cmd, "setenv -f mmcbootdev %x", mmc_get_bootdevindex());
	run_command(cmd, 0);
#endif
}
