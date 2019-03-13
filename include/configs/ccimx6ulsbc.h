/*
 * Copyright (C) 2016-2018 Digi International, Inc.
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Configuration settings for the Digi ConnecCore 6UL SBC board.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#ifndef CCIMX6ULSBC_CONFIG_H
#define CCIMX6ULSBC_CONFIG_H

#include "ccimx6ul_common.h"

#define CONFIG_BOARD_DESCRIPTION	"SBC Pro"

/* uncomment for PLUGIN mode support */
/* #define CONFIG_USE_PLUGIN */

/* uncomment for BEE support, needs to enable CONFIG_CMD_FUSE */
/* #define CONFIG_CMD_BEE */

/* FLASH and environment organization */
#if defined(CONFIG_NAND_BOOT)
#define CONFIG_SYS_USE_NAND
#define CONFIG_MTD_DEVICE
#define CONFIG_MTD_PARTITIONS
#define CONFIG_CMD_UPDATE_NAND
#define CONFIG_SYS_STORAGE_MEDIA	"nand"
#define CONFIG_CMD_BOOTSTREAM
#else
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

/* U-Boot Environment */
#if defined(CONFIG_ENV_IS_IN_MMC)
#define CONFIG_ENV_OFFSET		(8 * SZ_64K)
#elif defined(CONFIG_ENV_IS_IN_SPI_FLASH)
#define CONFIG_ENV_OFFSET		(768 * 1024)
#define CONFIG_ENV_SPI_BUS		CONFIG_SF_DEFAULT_BUS
#define CONFIG_ENV_SPI_CS		CONFIG_SF_DEFAULT_CS
#define CONFIG_ENV_SPI_MODE		CONFIG_SF_DEFAULT_MODE
#define CONFIG_ENV_SPI_MAX_HZ		CONFIG_SF_DEFAULT_SPEED
#elif defined(CONFIG_ENV_IS_IN_NAND)
#undef CONFIG_ENV_SIZE
#define CONFIG_ENV_OFFSET		(3 * SZ_1M)
#define CONFIG_ENV_SIZE			SZ_128K
#endif
#define CONFIG_SYS_REDUNDANT_ENVIRONMENT
#define CONFIG_DYNAMIC_ENV_LOCATION
#if (CONFIG_DDR_MB == 1024)
# define CONFIG_ENV_PARTITION_SIZE	(3 * SZ_1M)
#else
# define CONFIG_ENV_PARTITION_SIZE	(1 * SZ_1M)
#endif /* (CONFIG_DDR_MB == 1024) */
/* The environment may use any good blocks within the "environment" partition */
#define CONFIG_ENV_RANGE		CONFIG_ENV_PARTITION_SIZE

/* Serial port */
#define CONFIG_MXC_UART
#define CONFIG_MXC_UART_BASE		UART5_BASE
#undef CONFIG_CONS_INDEX
#define CONFIG_CONS_INDEX		5
#define CONSOLE_DEV			"ttymxc4"
#define CONFIG_BAUDRATE			115200

/* Ethernet */
#define CONFIG_FEC_ENET_DEV		0
#if (CONFIG_FEC_ENET_DEV == 0)
#define IMX_FEC_BASE			ENET_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x0
#define CONFIG_ETHPRIME                 "FEC0"
#define CONFIG_FEC_XCV_TYPE             RMII
#elif (CONFIG_FEC_ENET_DEV == 1)
#define IMX_FEC_BASE			ENET2_BASE_ADDR
#define CONFIG_FEC_MXC_PHYADDR          0x1
#define CONFIG_ETHPRIME                 "FEC1"
#define CONFIG_FEC_XCV_TYPE             RMII
#endif

/* Video */
#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_MXS
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define BACKLIGHT_GPIO			(IMX_GPIO_NR(4, 16))
#define BACKLIGHT_ENABLE_POLARITY	1
#endif

/* I2C */
#define CONFIG_SYS_I2C_MXC_I2C1
#define CONFIG_SYS_I2C_MXC_I2C2

#undef CONFIG_DEFAULT_FDT_FILE
#define CONFIG_DEFAULT_FDT_FILE		"zImage-imx6ul-" CONFIG_SYS_BOARD ".dtb"

#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#undef CONFIG_BOOTCOMMAND
#ifdef CONFIG_SECURE_BOOT
/*
 * Authenticate bootscript before running it. IVT offset is at
 * ${filesize} - CONFIG_CSF_SIZE - IVT_SIZE (0x20)
 * Use 0x4000 as CSF_SIZE, as this is the value used by the script
 * to sign / encrypt the bootscript
 */
#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"setexpr bs_ivt_offset ${filesize} - 0x4020;" \
		"if hab_auth_img ${loadaddr} ${bs_ivt_offset}; then " \
			"source ${loadaddr};" \
		"fi; " \
	"fi;"
#else
#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"source ${loadaddr};" \
	"fi;"

#endif	/* CONFIG_SECURE_BOOT */

#define CONFIG_COMMON_ENV	\
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	CONFIG_EXTRA_NETWORK_SETTINGS \
	"boot_fdt=yes\0" \
	"bootargs_mmc_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=${mmcroot} ${mtdparts}" \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_nfs=" \
		"if test ${ip_dyn} = yes; then " \
			"bootargs_ip=\"ip=dhcp\";" \
		"else " \
			"bootargs_ip=\"ip=\\${ipaddr}:\\${serverip}:" \
			"\\${gatewayip}:\\${netmask}:\\${hostname}:" \
			"eth0:off\";" \
		"fi;\0" \
	"bootargs_nfs_linux=run bootargs_nfs;" \
		"setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${mtdparts} ${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_tftp=" \
		"if test ${ip_dyn} = yes; then " \
			"bootargs_ip=\"ip=dhcp\";" \
		"else " \
			"bootargs_ip=\"ip=\\${ipaddr}:\\${serverip}:" \
			"\\${gatewayip}:\\${netmask}:\\${hostname}:" \
			"eth0:off\";" \
		"fi;\0" \
	"bootargs_tftp_linux=run bootargs_tftp;" \
		"setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${mtdparts} ${bootargs_once} ${extra_bootargs}\0" \
	"console=" CONSOLE_DEV "\0" \
	"dboot_kernel_var=zimage\0" \
	"fdt_addr=0x83000000\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_addr=0x83800000\0" \
	"initrd_file=uramdisk.img\0" \
	"initrd_high=0xffffffff\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"recovery_file=recovery.img\0" \
	"script=boot.scr\0" \
	"uboot_file=u-boot.imx\0" \
	"zimage=zImage-" CONFIG_SYS_BOARD ".bin\0"

#if defined(CONFIG_NAND_BOOT)
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	CONFIG_ENV_MTD_SETTINGS \
	"bootargs_linux=\0" \
	"bootargs_nand_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} ${mtdparts} ubi.mtd=${mtdlinuxindex} " \
		"ubi.mtd=${mtdrootfsindex} root=ubi1_0 " \
		"rootfstype=ubifs rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"install_linux_fw_sd=if load mmc 0 ${loadaddr} install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"linux_file=dey-image-qt-x11-" CONFIG_SYS_BOARD ".boot.ubifs\0" \
	"loadscript=" \
		"if test -z \"${mtdbootpart}\"; then " \
			"setenv mtdbootpart " CONFIG_LINUX_PARTITION ";" \
		"fi;" \
		"if ubi part ${mtdbootpart}; then " \
			"if ubifsmount ubi0:${mtdbootpart}; then " \
				"ubifsload ${loadaddr} ${script};" \
			"fi;" \
		"fi;\0" \
	"mtdbootpart=" CONFIG_LINUX_PARTITION "\0" \
	"mtdlinuxindex=" CONFIG_ENV_MTD_LINUX_INDEX "\0" \
	"mtdrecoveryindex=" CONFIG_ENV_MTD_RECOVERY_INDEX "\0" \
	"mtdrootfsindex=" CONFIG_ENV_MTD_ROOTFS_INDEX "\0" \
	"mtdupdateindex=" CONFIG_ENV_MTD_UPDATE_INDEX "\0" \
	"recoverycmd=" \
		"setenv mtdbootpart " CONFIG_RECOVERY_PARTITION ";" \
		"boot\0" \
	"rootfs_file=dey-image-qt-x11-" CONFIG_SYS_BOARD ".ubifs\0" \
	""	/* end line */
#else
#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	"loadscript=load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} " \
		"${mtdparts} " \
		"root=${mmcroot}\0" \
	""	/* end line */
#endif

#define CONFIG_SYS_MMC_ENV_DEV		0   /* USDHC2 */
#define CONFIG_SYS_MMC_ENV_PART		0	/* user area */
#define CONFIG_MMCROOT			"/dev/mmcblk1p2"  /* USDHC2 */

/* Carrier board version and ID commands */
#define CONFIG_CMD_BOARD_VERSION
#define CONFIG_CMD_BOARD_ID

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

/* UBI and UBIFS support */
#define CONFIG_DIGI_UBI

#endif /* CCIMX6ULSBC_CONFIG_H */
