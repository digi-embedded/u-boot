/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2025 Digi International Inc.
 */

#ifndef CCIMX95_DVK_H
#define CCIMX95_DVK_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>
#include <configs/ccimx9_common.h>

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 95"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"

/* Lock Fuses */
#define SMARCID_OCOTP_LOCK_BANK	1
#define SMARCID_OCOTP_LOCK_WORD	2
#define SMARCID_OCOTP_LOCK_FUSE	8

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

/* MCA */
#if IS_ENABLED(CONFIG_MCA)
#define BOARD_MCA_DEVICE_ID		0x69
#endif

/*
 * The first 256MB of DRAM (0x80000000 to 0x8fffffff) are located below the
 * u-boot base address. Optee currently allocates 32MB for its core and 2MB
 * for shared memory within the 0x8c000000 to 0x8e200000 range.
 */
#define CFG_SYS_INIT_RAM_ADDR        0x90000000
#define CFG_SYS_INIT_RAM_SIZE        0x200000

#define CFG_SYS_SDRAM_BASE           0x90000000
#define PHYS_SDRAM                      0x90000000
/* Up to 8GB */
#define PHYS_SDRAM_SIZE			0x70000000  /* 2GB  - 256MB DDR */
#define PHYS_SDRAM_2_SIZE 		0x180000000 /* 6GB */

#define CFG_SYS_SECURE_SDRAM_BASE	0x8A000000 /* Secure DDR region for A55, SPL could use first 2MB */
#define CFG_SYS_SECURE_SDRAM_SIZE	0x06000000

#define WDOG_BASE_ADDR			WDG3_BASE_ADDR

#ifdef CONFIG_ANDROID_SUPPORT
#include "imx95_evk_android.h"
#endif

#endif
