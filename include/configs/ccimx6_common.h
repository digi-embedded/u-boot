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

#ifndef CCIMX6_COMMON_CONFIG_H
#define CCIMX6_COMMON_CONFIG_H

#include <asm/arch/imx-regs.h>
#include <config_cmd_default.h>
#include <linux/sizes.h>
#include "mx6_common.h"
#include "digi_common.h"		/* Load Digi common stuff... */

#define CONFIG_MX6
#define CONFIG_CC6

#define CONFIG_SYS_GENERIC_BOARD
#define CONFIG_DISPLAY_CPUINFO
#define CONFIG_DISPLAY_BOARDINFO_LATE

#define CONFIG_CMDLINE_TAG
#define CONFIG_SETUP_MEMORY_TAGS
#define CONFIG_INITRD_TAG
#define CONFIG_REVISION_TAG

/*
 * RAM
 */
#define CONFIG_LOADADDR			0x12000000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_TEXT_BASE		0x17800000
/* RAM memory reserved for U-Boot, stack, malloc pool... */
#define CONFIG_UBOOT_RESERVED		(10 * 1024 * 1024)
/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		(CONFIG_ENV_SIZE + 2 * 1024 * 1024)
/* memtest */
#define CONFIG_SYS_MEMTEST_START       0x10000000
#define CONFIG_SYS_MEMTEST_END         0x10010000
#define CONFIG_SYS_MEMTEST_SCRATCH     0x10800000
/* Physical Memory Map */
#define CONFIG_NR_DRAM_BANKS           1
#define PHYS_SDRAM                     MMDC0_ARB_BASE_ADDR
#define CONFIG_SYS_SDRAM_BASE          PHYS_SDRAM
#define CONFIG_SYS_INIT_RAM_ADDR       IRAM_BASE_ADDR
#define CONFIG_SYS_INIT_RAM_SIZE       IRAM_SIZE

#define CONFIG_SYS_INIT_SP_OFFSET \
	(CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
	(CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_MXC_GPIO

/* Fuse */
#define CONFIG_CMD_FUSE
#ifdef CONFIG_CMD_FUSE
#define CONFIG_MXC_OCOTP
#endif

/* HWID */
#define CONFIG_HAS_HWID
#define CONFIG_HWID_BANK		4
#define CONFIG_HWID_START_WORD		2
#define CONFIG_HWID_WORDS_NUMBER	2
#define CONFIG_HWID_LOCK_FUSE		(1 << 8)
#define CONFIG_MANUF_STRINGS_HELP	"<LYYWWGGXXXXXX> <VVHC>"

#define OCOTP_LOCK_BANK		0
#define OCOTP_LOCK_WORD		0

/* CAAM support */
#define CONFIG_SYS_FSL_SEC_COMPAT    4 /* HAB version */
#define CONFIG_FSL_CAAM
#define CONFIG_CMD_DEKBLOB
#define CONFIG_SYS_FSL_SEC_LE

/*
 * Trustfence configs
 */
#define CONFIG_HAS_TRUSTFENCE
/* Secure boot configs */
#define CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS	3
#define CONFIG_TRUSTFENCE_SRK_REVOKE_BANK	5
#define CONFIG_TRUSTFENCE_SRK_REVOKE_WORD	7
#define CONFIG_TRUSTFENCE_SRK_REVOKE_MASK	0x7
#define CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET	0

#define CONFIG_TRUSTFENCE_SRK_BANK		3
#define CONFIG_TRUSTFENCE_SRK_WORDS		8

#define CONFIG_TRUSTFENCE_CLOSE_BIT_BANK	0
#define CONFIG_TRUSTFENCE_CLOSE_BIT_WORD	6
#define CONFIG_TRUSTFENCE_CLOSE_BIT_MASK	0x1
#define CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET	1

/* Secure JTAG configs */
#define CONFIG_TRUSTFENCE_JTAG_MODE_BANK		0
#define CONFIG_TRUSTFENCE_JTAG_MODE_START_WORD		6
#define CONFIG_TRUSTFENCE_JTAG_MODE_WORDS_NUMBER	1
#define CONFIG_TRUSTFENCE_JTAG_KEY_BANK		4
#define CONFIG_TRUSTFENCE_JTAG_KEY_START_WORD		0
#define CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER	2
#define CONFIG_TRUSTFENCE_JTAG_LOCK_FUSE		(1 << 2)
#define CONFIG_TRUSTFENCE_JTAG_KEY_LOCK_FUSE		(1 << 6)

#define TRUSTFENCE_JTAG_DISABLE_OFFSET			20
#define TRUSTFENCE_JTAG_SMODE_OFFSET			22

#define TRUSTFENCE_JTAG_DISABLE_JTAG_MASK 		0x01
#define TRUSTFENCE_JTAG_SMODE_MASK 			0x03

#define TRUSTFENCE_JTAG_SMODE_ENABLE 			0x00
#define TRUSTFENCE_JTAG_SMODE_SECURE 			0x01
#define TRUSTFENCE_JTAG_SMODE_NO_DEBUG			0x03

#define TRUSTFENCE_JTAG_DISABLE_JTAG			(0x01 << TRUSTFENCE_JTAG_DISABLE_OFFSET)
#define TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE		(0x01 << TRUSTFENCE_JTAG_SMODE_OFFSET)
#define TRUSTFENCE_JTAG_DISABLE_DEBUG			(0x03 << TRUSTFENCE_JTAG_SMODE_OFFSET)

/* Serial port */
#define CONFIG_MXC_UART

/* MMC Configs */
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR      0

#define CONFIG_MMC
#define CONFIG_CMD_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_CMD_EMMC
#define CONFIG_SUPPORT_EMMC_BOOT
#define CONFIG_SUPPORT_EMMC_RPMB
#define CONFIG_SUPPORT_MMC_ECSD
#define CONFIG_BOUNCE_BUFFER
#define CONFIG_CMD_EXT2
#define CONFIG_CMD_FAT
#define CONFIG_CMD_FS_GENERIC
#define CONFIG_FAT_WRITE
#define CONFIG_CMD_EXT4
#define CONFIG_CMD_EXT4_WRITE
#define CONFIG_DOS_PARTITION
#define CONFIG_EFI_PARTITION
#define CONFIG_CMD_GPT
#define CONFIG_PARTITION_UUIDS
#define CONFIG_CMD_PART

/* MMC device and partition where U-Boot image is */
#define CONFIG_SYS_BOOT_PART_EMMC	1	/* Boot part 1 on eMMC */
#define CONFIG_SYS_BOOT_PART_OFFSET	SZ_1K
#define CONFIG_SYS_BOOT_PART_SIZE	(SZ_2M - CONFIG_SYS_BOOT_PART_OFFSET)

/* SATA Configs */
#if defined(CONFIG_MX6Q)
#define CONFIG_CMD_SATA
#endif

#ifdef CONFIG_CMD_SATA
#define CONFIG_DWC_AHSATA
#define CONFIG_SYS_SATA_MAX_DEVICE      1
#define CONFIG_DWC_AHSATA_PORT_ID       0
#define CONFIG_DWC_AHSATA_BASE_ADDR     SATA_ARB_BASE_ADDR
#define CONFIG_LBA48
#define CONFIG_LIBATA
#endif

/* Ethernet */
#define CONFIG_CMD_NET
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_ETHPRIME			"FEC"
#define CONFIG_NO_MAC_FROM_OTP
#define CONFIG_PHYLIB
#define CONFIG_ARP_TIMEOUT     200UL

/* protected environment variables (besides ethaddr and serial#) */
#define CONFIG_ENV_FLAGS_LIST_STATIC	\
	"wlanaddr:mc,"			\
	"btaddr:mc,"			\
	"bootargs_once:sr,"		\
	"board_version:so,"		\
	"board_id:so,"			\
	"mmcbootdev:so"

#define CONFIG_CONS_INDEX              1
#define CONFIG_BAUDRATE                        115200
#define CONFIG_SILENT_CONSOLE_UPDATE_ON_RELOC

#define CONFIG_OF_LIBFDT
#define CONFIG_OF_BOARD_SETUP

#define CONFIG_CMD_BMODE
#define CONFIG_CMD_BOOTZ
#undef CONFIG_CMD_IMLS
#define CONFIG_CMD_SETEXPR

#ifndef CONFIG_SYS_DCACHE_OFF
#define CONFIG_CMD_CACHE
#endif

/* I2C configs */
#define CONFIG_SYS_I2C
#define CONFIG_CMD_I2C
#define CONFIG_SYS_I2C_MXC
#define CONFIG_I2C_MULTI_BUS
#define CONFIG_SYS_I2C_SPEED            100000

/* PMIC */
#define CONFIG_PMIC_I2C_BUS		1	/* DA9063 PMIC i2c bus */
#define CONFIG_PMIC_I2C_ADDR		0x58	/* DA9063 PMIC i2c address */
#define CONFIG_CMD_PMIC
#define CONFIG_PMIC_NUMREGS		0x185

/* FLASH and environment organization */
#define CONFIG_SYS_NO_FLASH

#if defined CONFIG_SYS_BOOT_SPINOR
#define CONFIG_SYS_USE_SPINOR
#define CONFIG_ENV_IS_IN_SPI_FLASH
#elif defined CONFIG_SYS_BOOT_EIMNOR
#define CONFIG_SYS_USE_EIMNOR
#define CONFIG_ENV_IS_IN_FLASH
#elif defined CONFIG_SYS_BOOT_NAND
#define CONFIG_SYS_USE_NAND
#define CONFIG_ENV_IS_IN_NAND
#elif defined CONFIG_SYS_BOOT_SATA
#define CONFIG_ENV_IS_IN_SATA
#define CONFIG_CMD_SATA
#else
#define CONFIG_ENV_IS_IN_MMC
#endif

#ifdef CONFIG_SYS_USE_SPINOR
#define CONFIG_CMD_SF
#define CONFIG_SPI_FLASH
#define CONFIG_SPI_FLASH_STMICRO
#define CONFIG_MXC_SPI
#define CONFIG_SF_DEFAULT_BUS  0
#define CONFIG_SF_DEFAULT_SPEED 20000000
#define CONFIG_SF_DEFAULT_MODE (SPI_MODE_0)
#endif

#ifdef CONFIG_SYS_USE_EIMNOR
#undef CONFIG_SYS_NO_FLASH
#define CONFIG_SYS_FLASH_BASE           WEIM_ARB_BASE_ADDR
#define CONFIG_SYS_FLASH_SECT_SIZE	(128 * 1024)
#define CONFIG_SYS_MAX_FLASH_BANKS 1    /* max number of memory banks */
#define CONFIG_SYS_MAX_FLASH_SECT 256   /* max number of sectors on one chip */
#define CONFIG_SYS_FLASH_CFI            /* Flash memory is CFI compliant */
#define CONFIG_FLASH_CFI_DRIVER         /* Use drivers/cfi_flash.c */
#define CONFIG_SYS_FLASH_USE_BUFFER_WRITE /* Use buffered writes*/
#define CONFIG_SYS_FLASH_EMPTY_INFO
#define CONFIG_SYS_FLASH_PROTECTION
#endif

#ifdef CONFIG_SYS_USE_NAND
#define CONFIG_CMD_NAND
#define CONFIG_CMD_NAND_TRIMFFS

/* NAND stuff */
#define CONFIG_NAND_MXS
#define CONFIG_SYS_MAX_NAND_DEVICE	1
#define CONFIG_SYS_NAND_BASE		0x40000000
#define CONFIG_SYS_NAND_5_ADDR_CYCLE
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* DMA stuff, needed for GPMI/MXS NAND support */
#define CONFIG_APBH_DMA
#define CONFIG_APBH_DMA_BURST
#define CONFIG_APBH_DMA_BURST8
#endif

#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_ENV_OFFSET		(1792 * 1024)	/* 256kB below 2MiB */
#define CONFIG_ENV_OFFSET_REDUND	(CONFIG_ENV_OFFSET + (128 * 1024))
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE
/* Default MMC device index/partition for location of environment */
#define CONFIG_SYS_MMC_ENV_DEV		0
#define CONFIG_SYS_MMC_ENV_PART		1
#elif defined(CONFIG_ENV_IS_IN_SPI_FLASH)
#define CONFIG_ENV_OFFSET		(768 * 1024)
#define CONFIG_ENV_SECT_SIZE		(8 * 1024)
#define CONFIG_ENV_SPI_BUS		CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS		CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE		CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED
#elif defined(CONFIG_ENV_IS_IN_FLASH)
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_SIZE			CONFIG_SYS_FLASH_SECT_SIZE
#define CONFIG_ENV_SECT_SIZE		CONFIG_SYS_FLASH_SECT_SIZE
#define CONFIG_ENV_OFFSET		(4 * CONFIG_SYS_FLASH_SECT_SIZE)
#elif defined(CONFIG_ENV_IS_IN_NAND)
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		(8 << 20)
#define CONFIG_ENV_SECT_SIZE		(128 << 10)
#define CONFIG_ENV_SIZE			CONFIG_ENV_SECT_SIZE
#elif defined(CONFIG_ENV_IS_IN_SATA)
#define CONFIG_ENV_OFFSET		(768 * 1024)
#define CONFIG_SATA_ENV_DEV		0
#define CONFIG_SYS_DCACHE_OFF /* remove when sata driver support cache */
#endif

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
#define CONFIG_DBOOT_BOOTCOMMAND	"bootm"
#define CONFIG_DBOOT_DEFAULTKERNELVAR	"uimage"
#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP

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

/* Pool of randomly generated UUIDs at host machine */
#define RANDOM_UUIDS	\
	"uuid_disk=075e2a9b-6af6-448c-a52a-3a6e69f0afff\0" \
	"part1_uuid=43f1961b-ce4c-4e6c-8f22-2230c5d532bd\0" \
	"part2_uuid=f241b915-4241-47fd-b4de-ab5af832a0f6\0" \
	"part3_uuid=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"part4_uuid=c7d8648b-76f7-4e2b-b829-e95a83cc7b32\0" \
	"part5_uuid=ebae5694-6e56-497c-83c6-c4455e12d727\0" \
	"part6_uuid=3845c9fc-e581-49f3-999f-86c9bab515ef\0" \
	"part7_uuid=3fcf7bf1-b6fe-419d-9a14-f87950727bc0\0" \
	"part8_uuid=12c08a28-fb40-430a-a5bc-7b4f015b0b3c\0" \
	"part9_uuid=dc83dea8-c467-45dc-84eb-5e913daec17e\0"

/* Helper strings for extra env settings */
#define CALCULATE_FILESIZE_IN_BLOCKS	\
	"setexpr filesizeblks ${filesize} / 200; " \
	"setexpr filesizeblks ${filesizeblks} + 1; "

#define CONFIG_ANDROID_RECOVERY

/* Miscellaneous configurable options */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2     "> "
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_CBSIZE              1024
#define CONFIG_SYS_HZ                  1000
#define CONFIG_CMDLINE_EDITING
#define CONFIG_SILENT_CONSOLE_UPDATE_ON_RELOC

#endif	/* CCIMX6_COMMON_CONFIG_H */
