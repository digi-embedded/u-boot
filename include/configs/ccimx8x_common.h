/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_COMMON_H
#define CCIMX8X_COMMON_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>
#include "digi_common.h"		/* Load Digi common stuff... */

#define CONFIG_CC8
#define CONFIG_SOM_DESCRIPTION		"ConnectCore 8X"

#define CONFIG_DISPLAY_BOARDINFO_LATE

/* RAM */
#define CONFIG_LOADADDR			0x80280000
#define CONFIG_SYS_TEXT_BASE		0x80020000
#define CONFIG_SYS_LOAD_ADDR		CONFIG_LOADADDR
#define CONFIG_SYS_INIT_SP_ADDR		0x80200000
/* RAM memory reserved for U-Boot, stack, malloc pool... */
#define CONFIG_UBOOT_RESERVED		(10 * 1024 * 1024)
/* RAM memory reserved for M4 (empirically determined) */
#define RESERVED_MEM_START		0xBC000000
#define RESERVED_MEM_END		0xC3000000
/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (32*1024)) * 1024)
/* memtest */
#define CONFIG_SYS_MEMTEST_START	0x90000000
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_MEMTEST_START + SZ_512M)
/* Physical Memory Map */
#define CONFIG_SYS_SDRAM_BASE		0x80000000
#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM_1			0x80000000
#define PHYS_SDRAM_1_SIZE		(CONFIG_DDR_MB * (unsigned int)SZ_1M)
#define PHYS_SDRAM			PHYS_SDRAM_1

#define CONFIG_OF_SYSTEM_SETUP
#define BOOTAUX_RESERVED_MEM_BASE 0x88000000
#define BOOTAUX_RESERVED_MEM_SIZE 0x08000000 /* Reserve from second 128MB */

/* HWID */
#define CONFIG_HAS_HWID
#define CONFIG_HWID_BANK		0
#define CONFIG_HWID_START_WORD		708
#define CONFIG_HWID_WORDS_NUMBER	3
#define CONFIG_NO_MAC_FROM_OTP

/* Media type for firmware updates */
#define CONFIG_SYS_STORAGE_MEDIA       "mmc"

/* MMC Configs */
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC
#define CONFIG_SYS_FSL_ESDHC_ADDR       0
#define USDHC1_BASE_ADDR                0x5B010000
#define USDHC2_BASE_ADDR                0x5B020000
#define CONFIG_SUPPORT_EMMC_BOOT	/* eMMC specific */
#define CONFIG_SUPPORT_MMC_ECSD
#define CONFIG_FAT_WRITE

/* MMC device and partition where U-Boot image is */
#define CONFIG_SYS_BOOT_PART_EMMC	1	/* Boot part 1 on eMMC */
#define CONFIG_SYS_BOOT_PART_OFFSET	(33 * SZ_1K)
#define CONFIG_SYS_BOOT_PART_SIZE	(SZ_2M - CONFIG_SYS_BOOT_PART_OFFSET)

/* Extra network settings for second Ethernet */
#define CONFIG_EXTRA_NETWORK_SETTINGS \
	"eth1addr=" DEFAULT_MAC_ETHADDR1 "\0"

/* protected environment variables (besides ethaddr and serial#) */
#define CONFIG_ENV_FLAGS_LIST_STATIC	\
	"eth1addr:mc,"			\
	"wlanaddr:mc,"			\
	"wlan1addr:mc,"			\
	"wlan2addr:mc,"			\
	"wlan3addr:mc,"			\
	"btaddr:mc,"			\
	"bootargs_once:sr,"		\
	"board_version:so,"		\
	"board_id:so,"			\
	"mmcbootdev:so"

/* Environment */
#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_ENV_OFFSET		(1792 * 1024)	/* 256kB below 2MiB */
#define CONFIG_ENV_OFFSET_REDUND	(CONFIG_ENV_OFFSET + (128 * 1024))
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE
/* On CC8X, USDHC1 is for eMMC, USDHC2 is for SD on SBC Express */
#define CONFIG_SYS_MMC_ENV_DEV		0	/* USDHC1 */
#define CONFIG_SYS_MMC_ENV_PART		2
#define CONFIG_SYS_MMC_IMG_LOAD_PART	1
#define CONFIG_SYS_FSL_USDHC_NUM	2

/* FUSE command */
#define CONFIG_CMD_FUSE

/* MCA */
#define CONFIG_MCA_I2C_BUS		0
#define CONFIG_MCA_I2C_ADDR		0x63

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

/* Monitor Command Prompt */
#define CONFIG_SYS_LONGHELP
#define CONFIG_SYS_PROMPT_HUSH_PS2     "> "
#define CONFIG_AUTO_COMPLETE
#define CONFIG_SYS_CBSIZE              1024
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					sizeof(CONFIG_SYS_PROMPT) + 16)
#define CONFIG_CMDLINE_EDITING

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS
#undef CONFIG_CMD_CRC32
#undef CONFIG_BOOTM_NETBSD

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		8000000	/* 8MHz */

#define CONFIG_IMX_SMMU

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
	"part9_uuid=dc83dea8-c467-45dc-84eb-5e913daec17e\0" \
	"part10_uuid=df0dba76-d5e0-11e8-9f8b-f2801f1b9fd1\0"

#define LINUX_4GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=1536MiB,uuid=${part3_uuid};" \
	"name=update,size=1536MiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_8GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=3GiB,uuid=${part3_uuid};" \
	"name=update,size=3GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_16GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=7GiB,uuid=${part3_uuid};" \
	"name=update,size=7GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

/* Partition defines */
#define CONFIG_RECOVERY_PARTITION	"2"

#endif /* CCIMX8X_COMMON_H */
