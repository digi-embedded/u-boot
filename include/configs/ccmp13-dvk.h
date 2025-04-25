/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2022-2023, Digi International Inc
 * Copyright (C) 2022, STMicroelectronics - All Rights Reserved
 *
 */

#ifndef __CCMP13_DVK_CONFIG_H__
#define __CCMP13_DVK_CONFIG_H__

#include <configs/ccmp1_common.h>

#define CONFIG_SOM_DESCRIPTION		"ConnectCore MP13"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"
#define BOARD_DEY_NAME			"ccmp13-dvk"

/* Serial */
#define CONSOLE_DEV			"ttySTM0"

#define CONFIG_MMCROOT		"/dev/mmcblk1p9"	/* microSD rootfs partition */
#define MMCDEV_DEFAULT		"1"	/* microSD dev index */
#define MMCPART_DEFAULT		"8"	/* default microSD partition index */

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CONFIG_COMMON_ENV	\
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	CONFIG_EXTRA_NETWORK_SETTINGS \
	ALTBOOTCMD \
	"bootcmd_mfg=fastboot " __stringify(CONFIG_FASTBOOT_USB_DEV) "\0" \
	"dualboot=yes\0" \
	"boot_fdt=yes\0" \
	"bootargs_linux=fbcon=logo-pos:center fbcon=logo-count:1\0" \
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
	"fdt_addr=0xc4000000\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_addr=0xc4400000\0" \
	"initrd_file=uramdisk.img\0" \
	"initrd_high=0xffffffff\0" \
	"install_linux_fw_sd=if load mmc 1 ${loadaddr} install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"install_linux_fw_usb=usb start;" \
		"if load usb 0 ${loadaddr} install_linux_fw_usb.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"usb_pgood_delay=2000\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcdev=" MMCDEV_DEFAULT "\0" \
	"mmcpart=" MMCPART_DEFAULT "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"linux_file=core-image-base-" CONFIG_SYS_BOARD ".boot.ubifs\0" \
	"recovery_file=recovery.img\0" \
	"rootfs_file=core-image-base-" CONFIG_SYS_BOARD ".ubifs\0" \
	"script=boot.scr\0" \
	"fit-script=bootscr-boot.txt\0" \
	"uboot_file=u-boot.imx\0" \
	"zimage=zImage-" BOARD_DEY_NAME ".bin\0" \
	"fitimage=fitImage-" BOARD_DEY_NAME ".bin\0"

#define ROOTARGS_UBIFS \
	"ubi.mtd=" SYSTEM_PARTITION " " \
	"ubi.mtd=" SYSTEM_PARTITION "_2 " \
	"root=ubi1:${rootfsvol} " \
	"rootfstype=ubifs rw"
#define ROOTARGS_SQUASHFS_A_PARTITION \
	"ubi.mtd=" SYSTEM_PARTITION " " \
	"ubi.mtd=" SYSTEM_PARTITION "_2 " \
	"ubi.block=1,${rootfsvol} root=/dev/ubiblock1_0 " \
	"rootfstype=squashfs ro"
#define ROOTARGS_SQUASHFS_B_PARTITION \
	"ubi.mtd=" SYSTEM_PARTITION " " \
	"ubi.mtd=" SYSTEM_PARTITION "_2 " \
	"ubi.block=1,${rootfsvol} root=/dev/ubiblock1_1 " \
	"rootfstype=squashfs ro"

#define MTDPART_ENV_SETTINGS \
	"mtdbootpart=" LINUX_A_PARTITION "\0" \
	"rootfsvol=" ROOTFS_A_PARTITION "\0" \
	"bootargs_nand_linux=" \
		"if test \"${rootfstype}\" = squashfs; then " \
			"if test \"${dualboot}\" = yes; then " \
				"if test \"${active_system}\" = linux_a; then " \
					"setenv rootargs " ROOTARGS_SQUASHFS_A_PARTITION ";" \
				"else " \
					"setenv rootargs " ROOTARGS_SQUASHFS_B_PARTITION ";" \
				"fi;" \
			"else " \
				"setenv rootargs " ROOTARGS_SQUASHFS_A_PARTITION ";" \
			"fi;" \
		"else " \
			"setenv rootargs " ROOTARGS_UBIFS ";" \
		"fi;" \
		"setenv bootargs console=${console},${baudrate} " \
			"${bootargs_linux} ${mtdparts} " \
			"${rootargs} " \
			"${bootargs_once} ${extra_bootargs};\0" \
	"loadscript=" \
		"if test \"${dualboot}\" = yes; then " \
			"if test -z \"${active_system}\"; then " \
				"setenv active_system " LINUX_A_PARTITION ";" \
			"fi;" \
			"setenv mtdbootpart ${active_system};" \
		"else " \
			"if test -z \"${mtdbootpart}\" || " \
			"   test \"${mtdbootpart}\" = " LINUX_A_PARTITION " || " \
			"   test \"${mtdbootpart}\" = " LINUX_B_PARTITION "; then " \
				"setenv mtdbootpart " LINUX_PARTITION ";" \
			"fi;" \
		"fi;" \
		"if test \"${boot_device}\" = mmc; then " \
			"load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};" \
		"else " \
			"if ubi part " SYSTEM_PARTITION "; then " \
				"if ubifsmount ubi0:${mtdbootpart}; then " \
					"if test \"${dboot_kernel_var}\" = fitimage; then " \
						"ubifsload ${loadaddr} ${fitimage};" \
					"else " \
						"ubifsload ${loadaddr} ${script};" \
					"fi;" \
				"fi;" \
			"fi;" \
		"fi;\0" \
	"recoverycmd=" \
		"setenv mtdbootpart " RECOVERY_PARTITION ";" \
		"boot\0"
#define DUALBOOT_ENV_SETTINGS \
	"linux_a=" LINUX_A_PARTITION "\0" \
	"linux_b=" LINUX_B_PARTITION "\0" \
	"rootfsvol_a=" ROOTFS_A_PARTITION "\0" \
	"rootfsvol_b=" ROOTFS_B_PARTITION "\0" \
	"active_system=" LINUX_A_PARTITION "\0"

#ifdef CONFIG_CONSOLE_DISABLE
#define ENV_SILENT_CONSOLE \
	"silent=yes\0" \
	"silent_linux=yes\0"
#else
#define ENV_SILENT_CONSOLE
#endif

#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	MTDPART_ENV_SETTINGS \
	DUALBOOT_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
	ENV_SILENT_CONSOLE \
	BOOTENV

#undef CONFIG_BOOTCOMMAND
#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"if test \"${dboot_kernel_var}\" = fitimage; then " \
			"source ${loadaddr}:${fit-script};" \
		"else " \
			"source ${loadaddr};" \
		"fi;" \
	"fi;"

/* Ethernet */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY    (1 * SZ_1M)
#endif

/* MTD partition layout (after the boot partitions) */
#define MTDPARTS_256M		"25m(" SYSTEM_PARTITION "),-(" SYSTEM_PARTITION "_2)"
#define MTDPARTS_512M		"35m(" SYSTEM_PARTITION "),-(" SYSTEM_PARTITION "_2)"
#define MTDPARTS_1024M		"35m(" SYSTEM_PARTITION "),-(" SYSTEM_PARTITION "_2)"

/* UBI volumes layout */
#define UBIVOLS_UBOOTENV		"ubi create uboot_config 20000;" \
					"ubi create uboot_config_r 20000;"

#define UBIVOLS1_256MB			"ubi create " LINUX_PARTITION " 900000;" \
					"ubi create " RECOVERY_PARTITION ";"
#define UBIVOLS2_256MB			"ubi create " ROOTFS_PARTITION " 7000000;" \
					"ubi create " UPDATE_PARTITION " 6000000;" \
					"ubi create " DATA_PARTITION ";"

#define UBIVOLS1_512MB			"ubi create " LINUX_PARTITION " 1000000;" \
					"ubi create " RECOVERY_PARTITION ";"
#define UBIVOLS2_512MB			"ubi create " ROOTFS_PARTITION " 10000000;" \
					"ubi create " UPDATE_PARTITION " b800000;" \
					"ubi create " DATA_PARTITION ";"

#define UBIVOLS1_1024MB 		"ubi create " LINUX_PARTITION " 1000000;" \
					"ubi create " RECOVERY_PARTITION ";"
#define UBIVOLS2_1024MB 		"ubi create " ROOTFS_PARTITION " 29DA1000;" \
					"ubi create " UPDATE_PARTITION " 10000000;" \
					"ubi create " DATA_PARTITION ";"

#define UBIVOLS1_DUALBOOT_256MB		"ubi create " LINUX_A_PARTITION " b00000;" \
					"ubi create " LINUX_B_PARTITION ";"
#define UBIVOLS2_DUALBOOT_256MB		"ubi create " ROOTFS_A_PARTITION " 6800000;" \
					"ubi create " ROOTFS_B_PARTITION " 6800000;" \
					"ubi create " DATA_PARTITION ";"

#define UBIVOLS1_DUALBOOT_512MB		"ubi create " LINUX_A_PARTITION " 1000000;" \
					"ubi create " LINUX_B_PARTITION ";"
#define UBIVOLS2_DUALBOOT_512MB		"ubi create " ROOTFS_A_PARTITION " dc00000;" \
					"ubi create " ROOTFS_B_PARTITION " dc00000;" \
					"ubi create " DATA_PARTITION ";"

#define UBIVOLS1_DUALBOOT_1024MB	"ubi create " LINUX_A_PARTITION " 1000000;" \
					"ubi create " LINUX_B_PARTITION ";"
#define UBIVOLS2_DUALBOOT_1024MB	"ubi create " ROOTFS_A_PARTITION " 1cecc400;" \
					"ubi create " ROOTFS_B_PARTITION " 1cecc400;" \
					"ubi create " DATA_PARTITION ";"

#define CREATE_UBIVOLS_SCRIPT		"ubi detach;" \
					"nand erase.part " SYSTEM_PARTITION ";" \
					"if test $? = 1; then " \
					"	echo \"** Error erasing '" SYSTEM_PARTITION "' partition\";" \
					"else" \
					"	ubi part " SYSTEM_PARTITION ";" \
					"	if test $? = 1; then " \
					"		echo \"Error attaching '" SYSTEM_PARTITION "' partition\";" \
					"	else " \
							UBIVOLS_UBOOTENV \
					"		if test \"${dualboot}\" = yes; then " \
					"			%s" \
					"		else " \
					"			%s" \
					"		fi;" \
					"	fi;" \
					"fi;" \
					"ubi detach;" \
					"nand erase.part " SYSTEM_PARTITION "_2;" \
					"if test $? = 1; then " \
					"	echo \"** Error erasing '" SYSTEM_PARTITION "_2' partition\";" \
					"else" \
					"	ubi part " SYSTEM_PARTITION "_2;" \
					"	if test $? = 1; then " \
					"		echo \"Error attaching '" SYSTEM_PARTITION "_2' partition\";" \
					"	else " \
					"		if test \"${dualboot}\" = yes; then " \
					"			%s" \
					"		else " \
					"			%s" \
					"		fi;" \
					"	fi;" \
					"fi;" \
					"saveenv"

#endif /* __CCMP13_DVK_CONFIG_H__ */
