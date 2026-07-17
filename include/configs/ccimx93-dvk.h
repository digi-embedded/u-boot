/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2022 NXP
 */

#ifndef CCIMX93_DVK_H
#define CCIMX93_DVK_H

#include <asm/arch/imx-regs.h>
#include <configs/ccimx9_common.h>

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 93"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CFG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

/* Link Definitions */
#define CFG_SYS_INIT_RAM_ADDR	0x80000000
#define CFG_SYS_INIT_RAM_SIZE	0x200000
#define CFG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM				0x80000000
#define PHYS_SDRAM_SIZE			0x40000000  /* 1GB DDR */

/* RAM memory reserved for OPTEE (32 MiB) */
#define RESERVED_MEM_START		0x96000000
#define RESERVED_MEM_END		0x98000000

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#endif
