/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2020 NXP
 */

#ifndef __IMX8ULP_WATCH_H
#define __IMX8ULP_WATCH_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>
#include <env/nxp/imx_env.h>

#ifdef CONFIG_XPL_BUILD

#define CFG_MALLOC_F_ADDR		0x22040000

#endif

#define CONFIG_SERIAL_TAG

/* ENET Config */
#if defined(CONFIG_FEC_MXC)
#define CFG_FEC_MXC_PHYADDR		1
#endif

/* Link Definitions */

#define CFG_SYS_INIT_RAM_ADDR	0x80000000
#define CFG_SYS_INIT_RAM_SIZE	0x80000

#define CFG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM			0x80000000
#define PHYS_SDRAM_SIZE			0x80000000 /* 2GB DDR */

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR			WDG3_RBASE

#ifdef CONFIG_ANDROID_SUPPORT
#include "imx8ulp_evk_android.h"
#endif

#endif
