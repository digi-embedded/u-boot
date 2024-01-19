/*
 * Copyright (C) 2018-2024 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_COMMON_H
#define CCIMX8X_COMMON_H

#include <linux/sizes.h>
#include <asm/arch/imx-regs.h>
#include "digi_common.h"		/* Load Digi common stuff... */

#define DIGI_IMX_FAMILY
#define CONFIG_SOM_DESCRIPTION		"ConnectCore 8X"

#ifdef CONFIG_SPL_BUILD
#define CFG_MALLOC_F_ADDR		0x00138000
/*
 * 0x08081000 - 0x08180FFF is for m4_0 xip image,
 * So 3rd container image may start from 0x8181000
 */
#define CFG_SYS_UBOOT_BASE 0x08181000
#endif /* CONFIG_SPL_BUILD */

/* Physical Memory Map */
#define CFG_SYS_SDRAM_BASE		0x80000000
#define PHYS_SDRAM_1			0x80000000
#ifdef CONFIG_SPL
/* SDRAM1 size is defined based on the HWID. Set here the default fallback value */
#define PHYS_SDRAM_1_SIZE		SZ_512M
#else
#define PHYS_SDRAM_1_SIZE		(CONFIG_DDR_MB * (unsigned int)SZ_1M)
#endif
#define PHYS_SDRAM_2			0x880000000
#define PHYS_SDRAM_2_SIZE		0
#define PHYS_SDRAM			PHYS_SDRAM_1

#define CONFIG_FSL_CAAM_KB

/* Secure boot configs */
#define CONFIG_TRUSTFENCE_SRK_BANK			0
#define CONFIG_TRUSTFENCE_SRK_WORDS			16
#define CONFIG_TRUSTFENCE_SRK_WORDS_PER_BANK		16
#define CONFIG_TRUSTFENCE_SRK_WORDS_OFFSET		730

#define CONFIG_TRUSTFENCE_SRK_N_REVOKE_KEYS		3
#define CONFIG_TRUSTFENCE_SRK_REVOKE_BANK		0
#define CONFIG_TRUSTFENCE_SRK_REVOKE_WORD		11
#define CONFIG_TRUSTFENCE_SRK_REVOKE_MASK		0xF
#define CONFIG_TRUSTFENCE_SRK_REVOKE_OFFSET		8

/* Secure JTAG configs
 * Currently not supported only place holder
 */
#define CONFIG_TRUSTFENCE_JTAG_MODE_BANK		0
#define CONFIG_TRUSTFENCE_JTAG_MODE_START_WORD		0
#define CONFIG_TRUSTFENCE_JTAG_MODE_WORDS_NUMBER	0
#define CONFIG_TRUSTFENCE_JTAG_KEY_BANK			0
#define CONFIG_TRUSTFENCE_JTAG_KEY_START_WORD		0
#define CONFIG_TRUSTFENCE_JTAG_KEY_WORDS_NUMBER		0
#define CONFIG_TRUSTFENCE_JTAG_LOCK_FUSE		0
#define CONFIG_TRUSTFENCE_JTAG_KEY_LOCK_FUSE		0

#define TRUSTFENCE_JTAG_DISABLE_OFFSET			0
#define TRUSTFENCE_JTAG_SMODE_OFFSET			0

#define TRUSTFENCE_JTAG_DISABLE_JTAG_MASK 		0
#define TRUSTFENCE_JTAG_SMODE_MASK 			0

#define TRUSTFENCE_JTAG_SMODE_ENABLE 			0
#define TRUSTFENCE_JTAG_SMODE_SECURE 			0
#define TRUSTFENCE_JTAG_SMODE_NO_DEBUG			0

#define TRUSTFENCE_JTAG_DISABLE_JTAG			0
#define TRUSTFENCE_JTAG_ENABLE_SECURE_JTAG_MODE		0
#define TRUSTFENCE_JTAG_DISABLE_DEBUG			0

#define OCOTP_LOCK_BANK		0
#define OCOTP_LOCK_WORD		0

/* Not used for this platform */
#define CONFIG_TRUSTFENCE_DIRBTDIS_BANK			0
#define CONFIG_TRUSTFENCE_DIRBTDIS_WORD			0
#define CONFIG_TRUSTFENCE_DIRBTDIS_OFFSET		0

#define CONFIG_TRUSTFENCE_CLOSE_BIT_BANK	0
#define CONFIG_TRUSTFENCE_CLOSE_BIT_WORD	0
#define CONFIG_TRUSTFENCE_CLOSE_BIT_OFFSET	0

/* MMC Configs */
#define CFG_SYS_FSL_ESDHC_ADDR       0
#define USDHC1_BASE_ADDR                0x5B010000
#define USDHC2_BASE_ADDR                0x5B020000
#define CONFIG_SUPPORT_MMC_ECSD
#define EMMC_BOOT_ACK			1
#define EMMC_BOOT_DEV			0
#define EMMC_BOOT_PART			1
#define EMMC_BOOT_PART_OFFSET_A0	(33 * SZ_1K)
#define EMMC_BOOT_PART_OFFSET		(32 * SZ_1K)

/* Ethernet */
#define WIRED_NICS			2

/* Extra network settings for second Ethernet */
#define CONFIG_EXTRA_NETWORK_SETTINGS \
	"eth1addr=" DEFAULT_MAC_ETHADDR1 "\0"

/* protected environment variables (besides ethaddr and serial#) */
#define CFG_ENV_FLAGS_LIST_STATIC	\
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
/* On CC8X, USDHC1 is for eMMC, USDHC2 is for SD on SBC Express */
#define CFG_SYS_FSL_USDHC_NUM	2

/* MCA */
#define BOARD_MCA_DEVICE_ID		0x4A

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

/* Digi boot command 'dboot' */
#define CONFIG_DBOOT_SUPPORTED_SOURCES_LIST	\
	CONFIG_SUPPORTED_SOURCES_NET "|" \
	CONFIG_SUPPORTED_SOURCES_BLOCK
#define CONFIG_DBOOT_SUPPORTED_SOURCES_ARGS_HELP	\
	DIGICMD_DBOOT_NET_ARGS_HELP "\n" \
	DIGICMD_DBOOT_BLOCK_ARGS_HELP

/* SUPPORT UPDATE */
#define CONFIG_TFTP_UPDATE_ONTHEFLY      /* support to tftp and update on-the-fly */

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

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS
#undef CONFIG_CMD_CRC32
#undef CONFIG_BOOTM_NETBSD

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		8000000	/* 8MHz */

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

#define LINUX_32GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=14GiB,uuid=${part3_uuid};" \
	"name=update,size=14GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define ANDROID_4GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=1024MiB,uuid=${part3_uuid};" \
	"name=cache,size=1024MiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

#define ANDROID_8GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=2GiB,uuid=${part3_uuid};" \
	"name=cache,size=2GiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

#define ANDROID_16GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=2GiB,uuid=${part3_uuid};" \
	"name=cache,size=2GiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

#define ANDROID_32GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=2GiB,uuid=${part3_uuid};" \
	"name=cache,size=2GiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
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

#define LINUX_DUALBOOT_16GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux_a,size=64MiB,uuid=${part1_uuid};" \
	"name=linux_b,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs_a,size=7GiB,uuid=${part3_uuid};" \
	"name=rootfs_b,size=7GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_DUALBOOT_32GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux_a,size=64MiB,uuid=${part1_uuid};" \
	"name=linux_b,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs_a,size=14GiB,uuid=${part3_uuid};" \
	"name=rootfs_b,size=14GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

/* Partition defines */
#define RECOVERY_PARTITION	"2"

#define FSL_FASTBOOT_FB_DEV "mmc"

/*
 * 'update' command will Ask for confirmation before updating any partition
 * in this comma-separated list
 */
#define SENSITIVE_PARTITIONS 	"uboot"

#endif /* CCIMX8X_COMMON_H */
