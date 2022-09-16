/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2022, Digi International Inc
 */

#ifndef __CONFIG_STM32MP13_ST_COMMON_H__
#define __CONFIG_STM32MP13_ST_COMMON_H__

#include <configs/ccmp1_common.h>

#define CONFIG_SOM_DESCRIPTION		"ConnectCore MP13"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"
#define BOARD_DEY_NAME			"ccmp13-dvk"

/* Serial */
#define CONSOLE_DEV			"ttySTM0"

#define CONFIG_MMCROOT			"/dev/mmcblk0p2"  /* USDHC1 */

#define CONFIG_COMMON_ENV	\
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	CONFIG_EXTRA_NETWORK_SETTINGS \
	ALTBOOTCMD \
	"bootcmd_mfg=fastboot " __stringify(CONFIG_FASTBOOT_USB_DEV) "\0" \
	"dualboot=no\0" \
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
	"fdt_addr=0xc4000000\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_high=0xffffffff\0"	  \
	"initrd_addr=0xc4400000\0" \
	"initrd_file=uramdisk.img\0" \
	"initrd_high=0xffffffff\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcroot=" CONFIG_MMCROOT " rootwait rw\0" \
	"recovery_file=recovery.img\0" \
	"script=boot.scr\0" \
	"uboot_file=u-boot.imx\0" \
	"zimage=zImage-" BOARD_DEY_NAME ".bin\0"

#define ROOTARGS_SINGLEMTDSYSTEM_UBIFS \
	"ubi.mtd=" SYSTEM_PARTITION " " \
	"root=ubi0:${rootfsvol} " \
	"rootfstype=ubifs rw"
#define ROOTARGS_MULTIMTDSYSTEM_UBIFS \
	"ubi.mtd=${mtdbootpart} " \
	"ubi.mtd=${mtdrootfspart} " \
	"root=ubi1:${rootfsvol} " \
	"rootfstype=ubifs rw"
#define ROOTARGS_SINGLEMTDSYSTEM_SQUASHFS \
	"ubi.mtd=" SYSTEM_PARTITION " " \
	"ubi.block=0,2 root=/dev/ubiblock0_2 " \
	"rootfstype=squashfs ro"
#define ROOTARGS_MULTIMTDSYSTEM_SQUASHFS \
	"ubi.mtd=${mtdbootpart} " \
	"ubi.mtd=${mtdrootfspart} " \
	"ubi.block=1,0 root=/dev/ubiblock1_0 " \
	"rootfstype=squashfs ro"

#define MTDPART_ENV_SETTINGS \
	"mtdbootpart=" LINUX_PARTITION "\0" \
	"mtdrootfspart=" ROOTFS_PARTITION "\0" \
	"singlemtdsys=yes\0" \
	"rootfsvol=" ROOTFS_PARTITION "\0" \
	"bootargs_nand_linux=" \
		"if test \"${singlemtdsys}\" = yes; then " \
			"if test \"${rootfstype}\" = squashfs; then " \
				"setenv rootargs " ROOTARGS_SINGLEMTDSYSTEM_SQUASHFS ";" \
			"else " \
				"setenv rootargs " ROOTARGS_SINGLEMTDSYSTEM_UBIFS ";" \
			"fi;" \
		"else " \
			"if test \"${rootfstype}\" = squashfs; then " \
				"setenv rootargs " ROOTARGS_MULTIMTDSYSTEM_SQUASHFS ";" \
			"else " \
				"setenv rootargs " ROOTARGS_MULTIMTDSYSTEM_UBIFS ";" \
			"fi;" \
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
			"if test -z \"${mtdbootpart}\"; then " \
				"setenv mtdbootpart " LINUX_PARTITION ";" \
			"fi;" \
		"fi;" \
		"if test \"${singlemtdsys}\" = yes; then " \
			"if ubi part " SYSTEM_PARTITION "; then " \
				"if ubifsmount ubi0:${mtdbootpart}; then " \
					"ubifsload ${loadaddr} ${script};" \
				"fi;" \
			"fi;" \
		"else " \
			"if ubi part ${mtdbootpart}; then " \
				"if ubifsmount ubi0:${mtdbootpart}; then " \
					"ubifsload ${loadaddr} ${script};" \
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

#define CONFIG_EXTRA_ENV_SETTINGS \
	CONFIG_COMMON_ENV \
	MTDPART_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
	BOOTENV \

#endif

/* Ethernet */
#ifdef CONFIG_DWC_ETH_QOS
#define CONFIG_SYS_NONCACHED_MEMORY    (1 * SZ_1M)
#endif
