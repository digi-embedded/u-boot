/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CCMP1_COMMON_H
#define __CCMP1_COMMON_H

#include <linux/sizes.h>
#include <asm/arch/stm32.h>
#include "digi_common.h"

#ifndef CONFIG_TFABOOT
/* PSCI support */
#define CONFIG_ARMV7_SECURE_BASE		STM32_SYSRAM_BASE
#define CONFIG_ARMV7_SECURE_MAX_SIZE		STM32_SYSRAM_SIZE
#endif

/*
 * Configuration of the external SRAM memory used by U-Boot
 */
#define CONFIG_SYS_SDRAM_BASE			STM32_DDR_BASE
#define CONFIG_SYS_INIT_SP_ADDR			CONFIG_SYS_TEXT_BASE

/*
 * Console I/O buffer size
 */
#define CONFIG_SYS_CBSIZE			SZ_1K

/*
 * default load address used for command tftp,  bootm , loadb, ...
 */
#define CONFIG_LOADADDR			0xc2000000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR

/* ATAGs */
#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG

/*
 * For booting Linux, use the first 256 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CONFIG_SYS_BOOTMAPSZ		SZ_256M

/* Extend size of kernel image for uncompression */
#define CONFIG_SYS_BOOTM_LEN		SZ_32M

/* SPL support */
#ifdef CONFIG_SPL
/* SPL use DDR */
#define CONFIG_SPL_BSS_START_ADDR	0xC0200000
#define CONFIG_SPL_BSS_MAX_SIZE		0x00100000
#define CONFIG_SYS_SPL_MALLOC_START	0xC0300000
#define CONFIG_SYS_SPL_MALLOC_SIZE	0x00100000

/* limit SYSRAM usage to first 128 KB */
#define CONFIG_SPL_MAX_SIZE		0x00020000
#define CONFIG_SPL_STACK		(STM32_SYSRAM_BASE + \
					 STM32_SYSRAM_SIZE)
#endif /* #ifdef CONFIG_SPL */

/* MCA */
#define BOARD_MCA_DEVICE_ID		0x61

/* Supported sources for update|dboot */
#if defined(CONFIG_CMD_DBOOT) || defined(CONFIG_CMD_UPDATE)
#define CONFIG_SUPPORTED_SOURCES	((1 << SRC_TFTP) | \
					 (1 << SRC_NFS) | \
					 (1 << SRC_MMC) | \
					 (1 << SRC_USB) | \
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
#define SYSTEM_PARTITION		"system"

/* Dualboot partition configuration */
#define LINUX_A_PARTITION		"linux_a"
#define LINUX_B_PARTITION		"linux_b"
#define ROOTFS_A_PARTITION		"rootfs_a"
#define ROOTFS_B_PARTITION		"rootfs_b"

#endif /* __CCMP1_COMMON_H */
