/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2013-2025 Digi International, Inc.
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
#include <linux/sizes.h>
#include "mx6_common.h"
#include "digi_common.h"		/* Load Digi common stuff... */

#define DIGI_IMX_FAMILY

#ifdef CONFIG_MX6QP
#define CONFIG_SOM_DESCRIPTION		"ConnectCore 6 Plus"
#else
#define CONFIG_SOM_DESCRIPTION		"ConnectCore 6"
#endif

/* Physical Memory Map */
#define PHYS_SDRAM                     MMDC0_ARB_BASE_ADDR
#define CFG_SYS_SDRAM_BASE		PHYS_SDRAM
#define CFG_SYS_INIT_RAM_ADDR	IRAM_BASE_ADDR
#define CFG_SYS_INIT_RAM_SIZE	IRAM_SIZE

/* Lock Fuses */
#define OCOTP_LOCK_BANK		0
#define OCOTP_LOCK_WORD		0

/* Secure boot configs */
#define CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS	3
#define CONFIG_TRUSTFENCE_SRK_REVOKE_BANK	5
#define CONFIG_TRUSTFENCE_SRK_REVOKE_WORD	7
#define CONFIG_TRUSTFENCE_SRK_REVOKE_MASK	0x7
#define CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET	0

#define CONFIG_TRUSTFENCE_SRK_BANK		3
#define CONFIG_TRUSTFENCE_SRK_WORDS		8
#define CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK	8
#define CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET	0

#define CONFIG_TRUSTFENCE_SRK_OTP_LOCK_BANK	0
#define CONFIG_TRUSTFENCE_SRK_OTP_LOCK_WORD	0
#define CONFIG_TRUSTFENCE_SRK_OTP_LOCK_OFFSET	14

#define CONFIG_TRUSTFENCE_CLOSE_BIT_BANK	0
#define CONFIG_TRUSTFENCE_CLOSE_BIT_WORD	6
#define CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET	1

#define CONFIG_TRUSTFENCE_DIRBTDIS_BANK		0
#define CONFIG_TRUSTFENCE_DIRBTDIS_WORD		6
#define CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET	3

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
#define TRUSTFENCE_JTAG_ENABLE_JTAG 			(TRUSTFENCE_JTAG_SMODE_ENABLE << TRUSTFENCE_JTAG_SMODE_OFFSET)
#define TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE		(TRUSTFENCE_JTAG_SMODE_SECURE << TRUSTFENCE_JTAG_SMODE_OFFSET)
#define TRUSTFENCE_JTAG_DISABLE_DEBUG			(TRUSTFENCE_JTAG_SMODE_NO_DEBUG << TRUSTFENCE_JTAG_SMODE_OFFSET)

/* MMC Configs */
#define CFG_SYS_FSL_ESDHC_ADDR      0
#define CFG_SYS_FSL_USDHC_NUM	2

#define CONFIG_SUPPORT_MMC_ECSD
#define EMMC_BOOT_ACK			1
#define EMMC_BOOT_DEV			0
#define EMMC_BOOT_PART			1
#define EMMC_BOOT_PART_OFFSET		SZ_1K

/* protected environment variables (besides ethaddr and serial#) */
#undef CFG_ENV_FLAGS_LIST_STATIC
#define CFG_ENV_FLAGS_LIST_STATIC	\
	"wlanaddr:mc,"			\
	"wlan1addr:mc,"			\
	"wlan2addr:mc,"			\
	"wlan3addr:mc,"			\
	"btaddr:mc,"			\
	"bootargs_once:sr,"		\
	"board_version:so,"		\
	"board_id:so,"			\
	"mmcbootdev:so"

/* PMIC */
#define CONFIG_PMIC_I2C_BUS		1	/* DA9063 PMIC i2c bus */
#define CONFIG_PMIC_I2C_ADDR		0x58	/* DA9063 PMIC i2c address */
#define CONFIG_PMIC_NUMREGS		0x185

#define CONFIG_TFTP_UPDATE_ONTHEFLY      /* support to tftp and update on-the-fly */

/* Supported sources for update|dboot */
#define CONFIG_SUPPORTED_SOURCES	((1 << SRC_TFTP) | \
					 (1 << SRC_NFS) | \
					 (1 << SRC_MMC) | \
					 (1 << SRC_USB) | \
					 (1 << SRC_RAM))
#define CONFIG_SUPPORTED_SOURCES_NET	"tftp|nfs"
#define CONFIG_SUPPORTED_SOURCES_BLOCK	"mmc|usb"
#define CONFIG_SUPPORTED_SOURCES_RAM	"ram"

#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP

/* Digi PMIC command 'pmic' */
#define CONFIG_CMD_DIGI_PMIC

/* Firmware update */
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

#define ALTBOOTCMD	\
	"altbootcmd=" \
		"if test \"${dualboot}\" = yes; then " \
			"if test \"${active_system}\" = linux_a; then " \
				"setenv active_system linux_b;" \
			"else " \
				"setenv active_system linux_a;" \
			"fi;"\
			"echo \"## System boot failed; Switching active partitions bank to ${active_system}...\";" \
		"fi;" \
		"bootcount reset;" \
		"reset;\0"

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

#define LINUX_DUALBOOT_4GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux_a,size=64MiB,uuid=${part1_uuid};" \
	"name=linux_b,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs_a,size=1536MiB,uuid=${part3_uuid};" \
	"name=rootfs_b,size=1536MiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_DUALBOOT_8GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux_a,size=64MiB,uuid=${part1_uuid};" \
	"name=linux_b,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs_a,size=3GiB,uuid=${part3_uuid};" \
	"name=rootfs_b,size=3GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

/* Partition defines */
#define BOOT_PARTITION		"1"
#define RECOVERY_PARTITION	"2"

/* Helper strings for extra env settings */
#define CALCULATE_FILESIZE_IN_BLOCKS	\
	"setexpr filesizeblks ${filesize} / 200; " \
	"setexpr filesizeblks ${filesizeblks} + 1; "

#define FSL_FASTBOOT_FB_DEV "mmc"

/*
 * 'update' command will Ask for confirmation before updating any partition
 * in this comma-separated list
 */
#define SENSITIVE_PARTITIONS 	"uboot"

#endif	/* CCIMX6_COMMON_CONFIG_H */
