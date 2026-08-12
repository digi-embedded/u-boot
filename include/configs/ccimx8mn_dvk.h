/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019-2026 Digi International Inc.
 */

#ifndef __CCIMX8MN_DVK_H
#define __CCIMX8MN_DVK_H

#include "ccimx8m_common.h"

/* RAM memory reserved for OPTEE (32 MiB) */
#define RESERVED_MEM_START		0x56000000
#define RESERVED_MEM_END		0x58000000

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 8M Nano"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"

#define EMMC_BOOT_PART_OFFSET		(32 * SZ_1K)

/* Serial */
#define CFG_MXC_UART_BASE	UART1_BASE_ADDR
#define CONSOLE_DEV			"ttymxc0"
#define EARLY_CONSOLE		"ec_imx6q,0x30860000"

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

#endif /* __CCIMX8MN_DVK_H */
