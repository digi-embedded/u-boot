/*
 * Copyright (C) 2016 Digi International, Inc.
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the Digi ConnecCore 6UL Starter board.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef CCIMX6ULSTARTER_CONFIG_H
#define CCIMX6ULSTARTER_CONFIG_H

#include "ccimx6ul_common.h"

#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_BOARD_DESCRIPTION	"ConnectCore 6UL StarterBoard"

/* uncomment for PLUGIN mode support */
/* #define CONFIG_USE_PLUGIN */

/* uncomment for SECURE mode support */
/* #define CONFIG_SECURE_BOOT */

/* uncomment for BEE support, needs to enable CONFIG_CMD_FUSE */
/* #define CONFIG_CMD_BEE */

#define CONFIG_NR_DRAM_BANKS		1
#define PHYS_SDRAM_SIZE			SZ_256M

/* FLASH and environment organization */
#define CONFIG_SYS_BOOT_NAND
#define CONFIG_SYS_NO_FLASH

#if defined(CONFIG_SYS_BOOT_NAND)
#define CONFIG_SYS_USE_NAND
#define CONFIG_ENV_IS_IN_NAND
#define CONFIG_CMD_MTDPARTS
#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS
#define CONFIG_CMD_UPDATE_NAND
#define CONFIG_SYS_STORAGE_MEDIA	"nand"
#define CONFIG_CMD_BOOTSTREAM
#else
#define CONFIG_ENV_IS_IN_MMC
#define CONFIG_CMD_UPDATE_MMC
#define CONFIG_SYS_STORAGE_MEDIA	"mmc"
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

#define CONFIG_SYS_FSL_USDHC_NUM	1

#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_ENV_OFFSET		(8 * SZ_64K)
#elif defined(CONFIG_ENV_IS_IN_SPI_FLASH)
#define CONFIG_ENV_OFFSET		(768 * 1024)
#define CONFIG_ENV_SECT_SIZE		(64 * 1024)
#define CONFIG_ENV_SPI_BUS		CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS		CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE		CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED
#elif defined(CONFIG_ENV_IS_IN_NAND)
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		(3 * SZ_1M)
#define CONFIG_ENV_SECT_SIZE		(128 << 10)
#define CONFIG_ENV_SIZE			CONFIG_ENV_SECT_SIZE
#endif

/* Serial port */
#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART5_BASE
#define CONFIG_CONS_INDEX		5
#define CONFIG_CONSOLE_PORT		"ttymxc4"
#define CONFIG_BAUDRATE			115200

#define CONFIG_MODULE_FUSE
#define CONFIG_OF_SYSTEM_SETUP

#define CONFIG_CMD_NET
#ifdef CONFIG_CMD_NET
#define CONFIG_CMD_PING
#define CONFIG_CMD_DHCP
#define CONFIG_CMD_MII
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define CONFIG_FEC_ENET_DEV		0

#if (CONFIG_FEC_ENET_DEV == 0)
#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x0
#define CONFIG_FEC_XCV_TYPE             RMII
#elif (CONFIG_FEC_ENET_DEV == 1)
#define IMX_FEC_BASE			ENET2_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x1
#define CONFIG_FEC_XCV_TYPE             RMII
#endif
#define CONFIG_ETHPRIME                 "FEC"

#define CONFIG_PHYLIB
#define CONFIG_PHY_SMSC
#define CONFIG_FEC_DMA_MINALIGN		64
#endif

#define CONFIG_DEFAULT_FDT_FILE		"zImage-imx6ul-" CONFIG_SYS_BOARD ".dtb"
#define CONFIG_BOOTARGS_CMA_SIZE	"cma=96M "

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"source ${loadaddr};" \
	"fi;"

#define CONFIG_COMMON_ENV	\
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	"boot_fdt=yes\0" \
	"console=" CONFIG_CONSOLE_PORT "\0" \
	"fdt_addr=0x83000000\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_addr=0x83800000\0" \
	"initrd_file=uramdisk.img\0" \
	"initrd_high=0xffffffff\0" \
	"script=boot.scr\0" \
	"uboot_file=u-boot.imx\0" \
	"zimage=zImage-" CONFIG_SYS_BOARD ".bin\0"

#if defined(CONFIG_SYS_BOOT_NAND)
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	CONFIG_ENV_MTD_SETTINGS \
	"bootargs_linux=" CONFIG_BOOTARGS_CMA_SIZE "\0" \
	"bootargs_nand_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=/dev/mtd${mtdrootfsindex} " \
		"${mtdparts} ubi.mtd=${mtdrootfsindex} root=ubi0_0 " \
		"rootfstype=ubifs rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"install_linux_fw_sd=if load mmc 0 ${loadaddr} install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"linux_file=core-image-base-" CONFIG_SYS_BOARD ".boot.ubifs\0" \
	"loadscript=ubi part " CONFIG_LINUX_PARTITION ";" \
		"ubifsmount ubi0:" CONFIG_LINUX_PARTITION";" \
		"ubifsload ${loadaddr} ${script}\0" \
	"mtdrootfsindex=" CONFIG_ENV_MTD_ROOTFS_INDEX "\0" \
	"rootfs_file=core-image-base-" CONFIG_SYS_BOARD ".ubifs\0" \
	""	/* end line */
#else
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	"loadscript=load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} " \
	    CONFIG_BOOTARGS_CMA_SIZE \
		"root=${mmcroot}\0" \
	""	/* end line */
#endif

#define CONFIG_SYS_MMC_ENV_DEV		0   /* USDHC2 */
#define CONFIG_SYS_MMC_ENV_PART		0	/* user area */
#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

/* Carrier board version in OTP bits */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
/* The carrier board version is stored in Bank 4 Word 6 (GP1)
 * in bits 3..0 */
#define CONFIG_CARRIERBOARD_VERSION_BANK	4
#define CONFIG_CARRIERBOARD_VERSION_WORD	6
#define CONFIG_CARRIERBOARD_VERSION_MASK	0xf
#define CONFIG_CARRIERBOARD_VERSION_OFFSET	0
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */

/* Carrier board ID in OTP bits */
#define CONFIG_HAS_CARRIERBOARD_ID
#ifdef CONFIG_HAS_CARRIERBOARD_ID
/* The carrier board ID is stored in Bank 4 Word 6 (GP1)
 * in bits 11..4 */
#define CONFIG_CARRIERBOARD_ID_BANK	4
#define CONFIG_CARRIERBOARD_ID_WORD	6
#define CONFIG_CARRIERBOARD_ID_MASK	0xff
#define CONFIG_CARRIERBOARD_ID_OFFSET	4
#endif /* CONFIG_HAS_CARRIERBOARD_ID */

#define CONFIG_BOOTDELAY               1

/* UBI and UBIFS support */
#define CONFIG_CMD_UBI
#define CONFIG_RBTREE
#define CONFIG_CMD_UBIFS
#define CONFIG_LZO
#define CONFIG_DIGI_UBI

#endif /* CCIMX6ULSTARTER_CONFIG_H */
