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

#include "ccimx6_common.h"
#include <asm/imx-common/gpio.h>

#define CONFIG_MACH_TYPE	4842
#define CONFIG_MXC_UART_BASE	UART1_BASE
#define CONFIG_CONSOLE_DEV		"ttymxc0"
#define CONFIG_MMCROOT			"/dev/mmcblk0p2"
#define CONFIG_DEFAULT_FDT_FILE		"imx6-ccimx6adpt-ldo.dtb"

#define CONFIG_SYS_FSL_USDHC_NUM	2
/* MMC device and partition where U-Boot image is */
#define CONFIG_SYS_MMC_BOOT_DEV		1	/* SDHC4 (eMMC) */
#define CONFIG_SYS_MMC_BOOT_PART	1	/* Boot part 1 */
/* MMC device where OS firmware files are */
#define CONFIG_SYS_MMC_IMG_LOAD_DEV	1	/* SDHC4 (eMMC) */

/* Ethernet PHY (select one) */
//#define CONFIG_PHY_MICREL
#define CONFIG_PHY_SMSC

#if defined(CONFIG_PHY_MICREL)
#define CONFIG_FEC_MXC_PHYADDR		3
#define CONFIG_FEC_XCV_TYPE		RGMII
#elif defined(CONFIG_PHY_SMSC)
#define CONFIG_FEC_MXC_PHYADDR		0
#define CONFIG_FEC_XCV_TYPE		RMII
#endif

#endif                         /* __CCIMX6ADPT_CONFIG_H */
