/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2013 Digi International, Inc.
 *
 * Configuration settings for the Freescale i.MX6Q SabreSD board.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the
 * GNU General Public License for more details.
 */

#ifndef __CCIMX6ADPTJS_CONFIG_H
#define __CCIMX6ADPT_CONFIG_H

#define CONFIG_MACH_TYPE	3980
#define CONFIG_MXC_UART_BASE	UART1_BASE
#define CONFIG_CONSOLE_DEV		"ttymxc0"
#define CONFIG_MMCROOT			"/dev/mmcblk0p2"

#include "ccimx6_common.h"
#include <asm/imx-common/gpio.h>

#define CONFIG_SYS_FSL_USDHC_NUM	3
/* MMC device and partition where U-Boot image is */
#define CONFIG_SYS_MMC_BOOT_DEV		2	/* SDHC4 (eMMC) */
#define CONFIG_SYS_MMC_BOOT_PART	1	/* Boot part 1 */
/* MMC device and partition where U-Boot environment is */
#define CONFIG_SYS_MMC_ENV_DEV		2	/* SDHC4 (eMMC) */
#define CONFIG_SYS_MMC_ENV_PART		2	/* Boot part 2 */
/* MMC device where OS firmware files are */
#define CONFIG_SYS_MMC_IMG_LOAD_DEV	1	/* SDHC3 */

#ifdef CONFIG_SYS_USE_SPINOR
#define CONFIG_SF_DEFAULT_CS   (0|(IMX_GPIO_NR(4, 9)<<8))
#endif

#endif                         /* __CCIMX6ADPT_CONFIG_H */
