/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2013-2018 Digi International, Inc.
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

#ifndef __CCIMX6SBC_CONFIG_H
#define __CCIMX6SBC_CONFIG_H

#include "ccimx6_common.h"
#include <asm/imx-common/gpio.h>

#define CONFIG_MACH_TYPE		4899

#ifdef CONFIG_MX6QP
#undef CONFIG_SYS_BOARD
#define CONFIG_SYS_BOARD		"ccimx6qpsbc"
#else
#endif
#define CONFIG_BOARD_DESCRIPTION	"SBC"

#define CONFIG_CONS_INDEX		1
#define CONFIG_MXC_UART_BASE		UART4_BASE
#define CONSOLE_DEV			"ttymxc3"
#define CONFIG_BAUDRATE			115200

#undef CONFIG_DEFAULT_FDT_FILE
#if defined(CONFIG_MX6DL) || defined(CONFIG_MX6S)
#define CONFIG_DEFAULT_FDT_FILE		"zImage-imx6dl-" CONFIG_SYS_BOARD ".dtb"
#elif defined(CONFIG_MX6QP)
#define CONFIG_DEFAULT_FDT_FILE		"zImage-imx6qp-" CONFIG_SYS_BOARD ".dtb"
#elif defined(CONFIG_MX6Q)
#define CONFIG_DEFAULT_FDT_FILE		"zImage-imx6q-" CONFIG_SYS_BOARD ".dtb"
#endif

#define CONFIG_SYS_FSL_USDHC_NUM	2

/* Media type for firmware updates */
#define CONFIG_SYS_STORAGE_MEDIA	"mmc"

/* Ethernet PHY */
#define CONFIG_PHY_MICREL
#define CONFIG_ENET_PHYADDR_MICREL	3
#define PHY_ANEG_TIMEOUT		8000

/* I2C */
#define CONFIG_SYS_I2C_MXC_I2C1
#define CONFIG_SYS_I2C_MXC_I2C2
#define CONFIG_SYS_I2C_MXC_I2C3
#define CONFIG_SYS_I2C_MXC_I2C4

/* Carrier board version and ID commands */
#define CONFIG_CMD_BOARD_VERSION
#define CONFIG_CMD_BOARD_ID

/* Carrier board version in OTP bits */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
/* For the SBC, the carrier board version is stored in Bank 4 Word 6 (GP1)
 * in bits 3..0 */
#define CONFIG_CARRIERBOARD_VERSION_BANK	4
#define CONFIG_CARRIERBOARD_VERSION_WORD	6
#define CONFIG_CARRIERBOARD_VERSION_MASK	0xf
#define CONFIG_CARRIERBOARD_VERSION_OFFSET	0
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */

/* Carrier board ID in OTP bits */
#define CONFIG_HAS_CARRIERBOARD_ID
#ifdef CONFIG_HAS_CARRIERBOARD_ID
/* For the SBC, the carrier board ID is stored in Bank 4 Word 6 (GP1)
 * in bits 11..4 */
#define CONFIG_CARRIERBOARD_ID_BANK	4
#define CONFIG_CARRIERBOARD_ID_WORD	6
#define CONFIG_CARRIERBOARD_ID_MASK	0xff
#define CONFIG_CARRIERBOARD_ID_OFFSET	4

/*
 * Custom carrier board IDs
 * Define here your custom carrier board ID numbers (between 1 and 127)
 * Use these defines to run conditional code basing on your carrier board
 * design.
 */

/* Digi ConnectCore 6 carrier board IDs */
#define CCIMX6SBC_ID129		129
#define CCIMX6SBC_ID130		130
#define CCIMX6SBC_ID131		131
#define CCIMX6QPSBC_ID160	160
#endif /* CONFIG_HAS_CARRIERBOARD_ID */

#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	RANDOM_UUIDS \
	ALTBOOTCMD \
	"bootcmd_mfg=fastboot " __stringify(CONFIG_FASTBOOT_USB_DEV) "\0" \
	"dualboot=no\0" \
	"bootlimit=0\0" \
	"dboot_kernel_var=zimage\0" \
	"script=boot.scr\0" \
	"loadscript=load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"uimage=uImage-" CONFIG_SYS_BOARD ".bin\0" \
	"zimage=zImage-" CONFIG_SYS_BOARD ".bin\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_addr=0x18000000\0" \
	"initrd_addr=0x19000000\0" \
	"initrd_file=uramdisk.img\0" \
	"boot_fdt=try\0" \
	"ip_dyn=yes\0" \
	"phy_mode=auto\0" \
	"console=" CONSOLE_DEV "\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_high=0xffffffff\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcbootpart=" __stringify(CONFIG_SYS_BOOT_PART_EMMC) "\0" \
	"mmcdev=0\0" \
	"mmcpart=" CONFIG_BOOT_PARTITION "\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} ${smp} " \
		"root=/dev/mmcblk0p2 rootwait rw\0" \
	"loaduimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${uimage}\0" \
	"loadzimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${zimage}\0" \
	"loadinitrd=load mmc ${mmcdev}:${mmcpart} ${initrd_addr} ${initrd_file}\0" \
	"loadfdt=load mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"uboot_file=u-boot.imx\0" \
	"boot_file=boot.img\0" \
	"system_file=system.img\0" \
	"partition_mmc_android=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"gpt write mmc ${mmcdev} ${parts_android};" \
			"mmc rescan;" \
		"fi;\0" \
	"bootargs_mmc_android=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_android} " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_tftp=" \
		"if test ${ip_dyn} = yes; then " \
			"bootargs_ip=\"ip=dhcp\";" \
		"else " \
			"bootargs_ip=\"ip=\\${ipaddr}:\\${serverip}:" \
			"\\${gatewayip}:\\${netmask}:\\${hostname}:" \
			"eth0:off\";" \
		"fi;\0" \
	"bootargs_tftp_android=run bootargs_tftp;" \
		"setenv bootargs console=${console},${baudrate} " \
		"${bootargs_android} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_nfs_android=run bootargs_tftp_android\0" \
	"mmcroot=PARTUUID=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"bootargs_mmc_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=${mmcroot} rootwait rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_tftp_linux=run bootargs_tftp;" \
		"setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_nfs_linux=run bootargs_tftp_linux\0" \
	"linux_file=dey-image-qt-xwayland-" CONFIG_SYS_BOARD ".boot.vfat\0" \
	"rootfs_file=dey-image-qt-xwayland-" CONFIG_SYS_BOARD ".ext4\0" \
	"partition_mmc_linux=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"if test \"${dualboot}\" = yes; then " \
				"gpt write mmc ${mmcdev} ${parts_linux_dualboot};" \
			"else " \
				"gpt write mmc ${mmcdev} ${parts_linux};" \
			"fi;" \
			"mmc rescan;" \
		"fi;\0" \
	"recoverycmd=setenv mmcpart " CONFIG_RECOVERY_PARTITION ";" \
		"boot\0" \
	"recovery_file=recovery.img\0" \
	"install_android_fw_sd=if load mmc 1 ${loadaddr} " \
		"install_android_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"install_linux_fw_sd=if load mmc 1 ${loadaddr} " \
		"install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"install_linux_fw_usb=usb start;" \
		"if load usb 0 ${loadaddr} install_linux_fw_usb.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"active_system=linux_a\0" \
	""	/* end line */

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

#endif                         /* __CCIMX6SBC_CONFIG_H */
