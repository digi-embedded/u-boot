/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * Copyright (C) 2018-2019, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2024, Digi International Inc - All Rights Reserved
 *
 * Configuration settings for the STM32MP25x CPU
 */

#ifndef __CONFIG_CCMP25_DVK_COMMMON_H
#define __CONFIG_CCMP25_DVK_COMMMON_H

#include <configs/ccmp2_common.h>

#define CONFIG_SOM_DESCRIPTION		"ConnectCore MP25"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"
#define BOARD_DEY_NAME			"ccmp25-dvk"

/* Serial */
#define CONSOLE_DEV			"ttySTM0"

#define MMCDEV_DEFAULT		"0"	/* eMMC dev index */
#define MMCPART_DEFAULT		"5"	/* default eMMC partition index */

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CONFIG_COMMON_ENV	\
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	CONFIG_EXTRA_NETWORK_SETTINGS \
	ALTBOOTCMD \
	"dboot_kernel_var=imagegz\0" \
	"dualboot=yes\0" \
	"bootcmd_mfg=fastboot " __stringify(CONFIG_FASTBOOT_USB_DEV) "\0" \
	"boot_fdt=yes\0" \
	"bootargs_linux=fbcon=logo-pos:center fbcon=logo-count:1\0" \
	"bootargs_mmc_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=${mmcroot} rootwait rw " \
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
		"${bootargs_once} ${extra_bootargs}\0" \
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
		"${bootargs_once} ${extra_bootargs}\0" \
	"console=" CONSOLE_DEV "\0" \
	"fdt_addr=0x88000000\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"fdt_high=0xffffffff\0"	  \
	"image=Image-" BOARD_DEY_NAME ".bin\0" \
	"imagegz=Image.gz-" BOARD_DEY_NAME ".bin\0" \
	"initrd_addr=0x88400000\0" \
	"initrd_file=uramdisk.img\0" \
	"initrd_high=0xffffffff\0" \
	"splashimage=0x90000000\0" \
	"splashpos=m,m\0" \
	"install_linux_fw_sd=if load mmc 2 ${loadaddr} install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"install_linux_fw_usb=usb start;" \
		"if load usb 0 ${loadaddr} install_linux_fw_usb.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"usb_pgood_delay=2000\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcbootpart=" __stringify(EMMC_BOOT_PART) "\0" \
	"mmcdev=" MMCDEV_DEFAULT "\0" \
	"mmcpart=" MMCPART_DEFAULT "\0" \
	"mmcroot=PARTUUID=3fcf7bf1-b6fe-419d-9a14-f87950727bc0\0" \
	"linux_file=dey-image-webkit-wayland-" CONFIG_SYS_BOARD ".boot.vfat\0" \
	"loadscript=" \
		"if test \"${dualboot}\" = yes; then " \
			"env exists active_system || setenv active_system linux_a; " \
			"part number mmc ${mmcbootdev} ${active_system} mmcpart; " \
		"fi;" \
		"load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"lzipaddr=" __stringify(CONFIG_DIGI_LZIPADDR) "\0" \
	"partition_mmc_linux=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"if test \"${dualboot}\" = yes; then " \
				"gpt write mmc ${mmcdev} ${parts_linux_dualboot};" \
			"else " \
				"gpt write mmc ${mmcdev} ${parts_linux};" \
			"fi;" \
			"mmc rescan;" \
		"fi;\0" \
	"recovery_file=recovery.img\0" \
	"rootfs_file=dey-image-webkit-wayland-" CONFIG_SYS_BOARD ".ext4\0" \
	"script=boot.scr\0" \
	"fit-script=bootscr-boot.txt\0" \
	"uboot_file=u-boot.imx\0" \
	"zimage=zImage-" BOARD_DEY_NAME ".bin\0" \
	"fitimage=fitImage-" BOARD_DEY_NAME ".bin\0" \
	"recoverycmd=""part number mmc ${mmcbootdev} recovery mmcpart; boot\0"


#define DUALBOOT_ENV_SETTINGS \
	"active_system=linux_a\0"

#include <config_distro_bootcmd.h>
#define CONFIG_EXTRA_ENV_SETTINGS \
	RANDOM_UUIDS \
	CONFIG_COMMON_ENV \
	DUALBOOT_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
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

#endif /* __CONFIG_CCMP25_DVK_COMMMON_H */
