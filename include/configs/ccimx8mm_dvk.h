/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2021-2026 Digi International Inc.
 */

#ifndef __CCIMX8MM_DVK_H
#define __CCIMX8MM_DVK_H

#include "ccimx8m_common.h"

/* RAM memory reserved for OPTEE (32 MiB) */
#define RESERVED_MEM_START		0x7E000000
#define RESERVED_MEM_END		0x80000000

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 8M Mini"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"

#ifdef CONFIG_XPL_BUILD
/* malloc f used before GD_FLG_FULL_MALLOC_INIT set */
#define CFG_MALLOC_F_ADDR		0x930000
#endif

#define EMMC_BOOT_PART_OFFSET		(33 * SZ_1K)

/* Serial */
#define CFG_MXC_UART_BASE		UART1_BASE_ADDR
#define CONSOLE_DEV			"ttymxc0"
#define EARLY_CONSOLE			"ec_imx6q,0x30860000"
#define CONFIG_BAUDRATE			115200

/* ENET Config */
#define PHY_ANEG_TIMEOUT	20000
#if defined(CONFIG_FEC_MXC)
#define CFG_FEC_MXC_PHYADDR	0
#endif

/* USDHC */
#define CFG_SYS_FSL_USDHC_NUM	2
#define CFG_SYS_FSL_ESDHC_ADDR	0

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#ifdef CONFIG_IMX_MATTER_TRUSTY
#define NS_ARCH_ARM64 1
#endif

/* Android specific configuration */
#ifdef CONFIG_ANDROID_SUPPORT
#include "ccimx8mm_dvk_android.h"
#endif

#endif /* __CCIMX8MM_DVK_H */
