/*
 * Copyright 2024 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CCMP2_COMMON_H
#define __CCMP2_COMMON_H

#include <linux/sizes.h>
#include <asm/arch/stm32.h>
#include "digi_common.h"

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

/* Ethernet need */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SERVERIP                 192.168.1.1
#define CONFIG_BOOTP_SERVERIP
#define CONFIG_SYS_AUTOLOAD		"no"
#endif

/*****************************************************************************/
#ifdef CONFIG_DISTRO_DEFAULTS
/*****************************************************************************/

#ifdef CONFIG_NET
#define BOOT_TARGET_PXE(func)	func(PXE, pxe, na)
#else
#define BOOT_TARGET_PXE(func)
#endif

#ifdef CONFIG_CMD_MMC
#define BOOT_TARGET_MMC0(func)	func(MMC, mmc, 0)
#define BOOT_TARGET_MMC1(func)	func(MMC, mmc, 1)
#define BOOT_TARGET_MMC2(func)	func(MMC, mmc, 2)
#else
#define BOOT_TARGET_MMC0(func)
#define BOOT_TARGET_MMC1(func)
#define BOOT_TARGET_MMC2(func)
#endif

#ifdef CONFIG_CMD_UBIFS
#define BOOT_TARGET_UBIFS(func)	func(UBIFS, ubifs, 0, UBI, boot)
#else
#define BOOT_TARGET_UBIFS(func)
#endif

#define BOOT_TARGET_DEVICES(func)	\
	BOOT_TARGET_MMC1(func)		\
	BOOT_TARGET_UBIFS(func)		\
	BOOT_TARGET_MMC0(func)		\
	BOOT_TARGET_MMC2(func)		\
	BOOT_TARGET_PXE(func)

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

#endif /* CONFIG_DISTRO_DEFAULTS */






/*
 * default load address used for command tftp,  bootm , loadb, ...
 */
#define CONFIG_LOADADDR			CONFIG_SYS_LOAD_ADDR

/* MMC Configs */
#define EMMC_BOOT_ACK			1
#define EMMC_BOOT_DEV			0
#define EMMC_BOOT_PART			1
#define EMMC_BOOT_PART_OFFSET		(32 * SZ_1K) /* Pending to check*/



/* Supported sources for update|dboot */
#if defined(CONFIG_CMD_DBOOT) || defined(CONFIG_CMD_UPDATE)
#define CONFIG_SUPPORTED_SOURCES	((1 << SRC_TFTP) | \
					 (1 << SRC_NFS) | \
					 (1 << SRC_MMC) | \
					 (1 << SRC_USB) | \
					 (1 << SRC_NAND) | \
					 (1 << SRC_RAM))
#define CONFIG_SUPPORTED_SOURCES_NET	"tftp|nfs"
#define CONFIG_SUPPORTED_SOURCES_BLOCK	"mmc|usb"
#define CONFIG_SUPPORTED_SOURCES_RAM	"ram"
#endif /* CONFIG_CMD_DBOOT || CONFIG_CMD_UPDATE */

/* Digi direct boot command 'dboot' */
#ifdef CONFIG_CMD_DBOOT
#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP
#endif /* CONFIG_CMD_DBOOT */

/* Firmware update */
#ifdef CONFIG_CMD_UPDATE
#define CONFIG_UPDATE_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK "|" \
	CONFIG_SUPPORTED_SOURCES_RAM
#define CONFIG_UPDATE_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_UPDATE_NET_ARGS_HELP "\n" \
	DIGICMD_UPDATE_BLOCK_ARGS_HELP "\n" \
	DIGICMD_UPDATE_RAM_ARGS_HELP
#define CONFIG_UPDATEFILE_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_UPDATEFILE_NET_ARGS_HELP "\n" \
	DIGICMD_UPDATEFILE_BLOCK_ARGS_HELP "\n" \
	DIGICMD_UPDATEFILE_RAM_ARGS_HELP
#endif /* CONFIG_CMD_UPDATE */

/* Partitions */
#define UBOOT_PARTITION			"bootloader"
#define LINUX_PARTITION			"linux"
#define RECOVERY_PARTITION		"recovery"
#define ROOTFS_PARTITION		"rootfs"
#define UPDATE_PARTITION		"update"
#define SYSTEM_PARTITION		"UBI"
#define DATA_PARTITION			"data"

#define NUM_SYSTEM_PARTITIONS		2

/* Dualboot partition configuration */
#define LINUX_A_PARTITION		"linux_a"
#define LINUX_B_PARTITION		"linux_b"
#define ROOTFS_A_PARTITION		"rootfs_a"
#define ROOTFS_B_PARTITION		"rootfs_b"

#define ALTBOOTCMD	\
	"altbootcmd=" \
		"if test \"${dualboot}\" = yes; then " \
			"if test \"${active_system}\" = linux_a; then " \
				"setenv active_system linux_b;" \
			"else " \
				"setenv active_system linux_a;" \
			"fi;" \
			"saveenv;" \
			"echo \"## System boot failed; Switching active partitions bank to ${active_system}...\";" \
		"fi;" \
		"bootcount reset;" \
		"reset;\0"

/* Extra network settings for second Ethernet */
#define CONFIG_EXTRA_NETWORK_SETTINGS \
	"eth1addr=" DEFAULT_MAC_ETHADDR1 "\0"

#define CONFIG_ENV_FLAGS_LIST_STATIC	\
	"eth1addr:mc,"			\
	"wlanaddr:mc,"			\
	"wlan1addr:mc,"			\
	"wlan2addr:mc,"			\
	"wlan3addr:mc,"			\
	"btaddr:mc,"			\
	"bootargs_once:sr,"		\
	"board_version:so,"		\
	"board_id:so,"

/*
 * 'update' command will Ask for confirmation before updating any partition
 * in this list
 */
#define SENSITIVE_PARTITIONS \
	"fsbl1," \
	"fsbl2," \
	"metadata1," \
	"metadata2," \
	"fip-a," \
	"fip-b"

#endif /* __CCMP2_COMMON_H */
