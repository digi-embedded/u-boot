/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * Copyright (C) 2018-2019, STMicroelectronics - All Rights Reserved
 *
 * Configuration settings for the STM32MP25x CPU
 */

#ifndef __CONFIG_STM32MP25_COMMMON_H
#define __CONFIG_STM32MP25_COMMMON_H
#include <linux/sizes.h>
#include <asm/arch/stm32.h>

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define CONFIG_SYS_SDRAM_BASE		STM32_DDR_BASE

/*
 * For booting Linux, use the first 256 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ		SZ_256M

/* MMC */
#define CONFIG_SYS_MMC_MAX_DEVICE	3

/* NAND support */
#define CONFIG_SYS_MAX_NAND_DEVICE	1

/* CFI support */
#define CONFIG_SYS_MAX_FLASH_BANKS	1
#define CONFIG_CFI_FLASH_USE_WEAK_ACCESSORS

/*****************************************************************************/
#ifdef CONFIG_DISTRO_DEFAULTS
/*****************************************************************************/

#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0)

#ifndef STM32MP_BOARD_EXTRA_ENV
#define STM32MP_BOARD_EXTRA_ENV
#endif

#define STM32MP_EXTRA \
	"env_check=if env info -p -d -q; then env save; fi\0" \
	"boot_net_usb_start=true\0"

#define __KERNEL_COMP_ADDR_R	__stringify(0x84000000)
#define __KERNEL_COMP_SIZE_R	__stringify(0x04000000)
#define __KERNEL_ADDR_R		__stringify(0x88000000)
#define __FDT_ADDR_R		__stringify(0x8a000000)
#define __SCRIPT_ADDR_R		__stringify(0x8a100000)
#define __PXEFILE_ADDR_R	__stringify(0x8a200000)
#define __FDTOVERLAY_ADDR_R	__stringify(0x8a300000)
#define __RAMDISK_ADDR_R	__stringify(0x8a400000)

#define STM32MP_MEM_LAYOUT \
	"kernel_addr_r=" __KERNEL_ADDR_R "\0" \
	"fdt_addr_r=" __FDT_ADDR_R "\0" \
	"scriptaddr=" __SCRIPT_ADDR_R "\0" \
	"pxefile_addr_r=" __PXEFILE_ADDR_R "\0" \
	"fdtoverlay_addr_r=" __FDTOVERLAY_ADDR_R "\0" \
	"ramdisk_addr_r=" __RAMDISK_ADDR_R "\0" \
	"kernel_comp_addr_r=" __KERNEL_COMP_ADDR_R "\0"	\
	"kernel_comp_size=" __KERNEL_COMP_SIZE_R "\0"

#include <config_distro_bootcmd.h>
#define CONFIG_EXTRA_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
	BOOTENV \
	STM32MP_EXTRA \
	STM32MP_BOARD_EXTRA_ENV

#endif

#endif /* __CONFIG_STM32MP25_COMMMON_H */
