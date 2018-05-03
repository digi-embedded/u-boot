/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8QXP_COMMON_H
#define CCIMX8QXP_COMMON_H

/* RAM */
#define CONFIG_LOADADDR			0x80280000
#define CONFIG_SYS_TEXT_BASE		0x80020000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_INIT_SP_ADDR		0x80200000
/* RAM memory reserved for U-Boot, stack, malloc pool... */
#define CONFIG_UBOOT_RESERVED		(10 * 1024 * 1024)

/* MMC device and partition where U-Boot image is */
#define CONFIG_SYS_BOOT_PART_EMMC	1	/* Boot part 1 on eMMC */
#define CONFIG_SYS_BOOT_PART_OFFSET	(33 * SZ_1K)
#define CONFIG_SYS_BOOT_PART_SIZE	(SZ_2M - CONFIG_SYS_BOOT_PART_OFFSET)

/* Media type for firmware updates */
#define CONFIG_SYS_STORAGE_MEDIA       "mmc"

/* MMC Configs */
#define CONFIG_SUPPORT_MMC_ECSD
#define CONFIG_FAT_WRITE

#define CONFIG_TFTP_UPDATE_ONTHEFLY      /* support to tftp and update on-the-fly */

/* Supported sources for update|dboot */
#define CONFIG_SUPPORTED_SOURCES	((1 << SRC_TFTP) | \
					 (1 << SRC_NFS) | \
					 (1 << SRC_MMC) | \
					 (1 << SRC_RAM))
#define CONFIG_SUPPORTED_SOURCES_NET	"tftp|nfs"
#define CONFIG_SUPPORTED_SOURCES_BLOCK	"mmc"
#define CONFIG_SUPPORTED_SOURCES_RAM	"ram"

/* Digi boot command 'dboot' */
#define CONFIG_CMD_DBOOT

#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP

/* SUPPORT UPDATE */
#define CONFIG_TFTP_UPDATE_ONTHEFLY      /* support to tftp and update on-the-fly */

/* Firmware update */
#define CONFIG_CMD_UPDATE_MMC
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
/* On the fly update chunk (must be a multiple of mmc block size) */
#define CONFIG_OTF_CHUNK		(32 * 1024 * 1024)

#endif /* CCIMX8QXP_COMMON_H */
