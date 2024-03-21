/*
 * Copyright 2021-2024 Digi International Inc
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CCIMX8MM_DVK_H
#define __CCIMX8MM_DVK_H

#include "ccimx8m_common.h"

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 8M Mini"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"
#define BOARD_DEY_NAME			"ccimx8mm-dvk"
#define PRODUCT_NAME			"ccimx8mmdvk"  /* (== TARGET_BOOTLOADER_BOARD_NAME in Android) */

#ifdef CONFIG_SPL_BUILD
/* malloc f used before GD_FLG_FULL_MALLOC_INIT set */
#define CFG_MALLOC_F_ADDR		0x930000
#endif

#define EMMC_BOOT_PART_OFFSET		(33 * SZ_1K)

/* Serial */
#define CFG_MXC_UART_BASE		UART1_BASE_ADDR
#define CONSOLE_DEV			"ttymxc0"
#define EARLY_CONSOLE			"ec_imx6q,0x30860000"
#define CONFIG_BAUDRATE			115200

/* ENET Config */
/* ENET1 */
#if defined(CONFIG_FEC_MXC)
#define CONFIG_MII
#define CONFIG_ETHPRIME                 "FEC"
#define PHY_ANEG_TIMEOUT 20000

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_MXC_PHYADDR          0
#define FEC_QUIRK_ENET_MAC

#define IMX_FEC_BASE			0x30BE0000
#endif

/* RAM */
#define PHYS_SDRAM_SIZE			0x80000000 /* 2GB DDR */
#define AUTODETECT_RAM_SIZE

/* USDHC */
#define CFG_SYS_FSL_USDHC_NUM	2
#define CFG_SYS_FSL_ESDHC_ADDR	0

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CFG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/ram0 rw quiet\0" \
	"fastboot_dev=mmc" __stringify(EMMC_BOOT_DEV) "\0" \
	"initrd_addr=0x43800000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=" __stringify(EMMC_BOOT_DEV) "\0" \
	"sd_dev=1\0"

/* Initial environment variables */
#define CFG_EXTRA_ENV_SETTINGS		\
	CFG_MFG_ENV_SETTINGS 		\
	CONFIG_DEFAULT_NETWORK_SETTINGS		\
	RANDOM_UUIDS \
	ALTBOOTCMD \
	"dualboot=no\0" \
	"dboot_kernel_var=imagegz\0" \
	"lzipaddr=" __stringify(CONFIG_DIGI_LZIPADDR) "\0" \
	"script=boot.scr\0" \
	"loadscript=" \
		"if test \"${dualboot}\" = yes; then " \
			"env exists active_system || setenv active_system linux_a; " \
			"part number mmc ${mmcbootdev} ${active_system} mmcpart; " \
		"fi;" \
		"load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"image=Image-" BOARD_DEY_NAME ".bin\0" \
	"imagegz=Image.gz-" BOARD_DEY_NAME ".bin\0" \
	"uboot_file=imx-boot-" BOARD_DEY_NAME ".bin\0" \
	"panel=NULL\0" \
	"splashimage=0x50000000\0" \
	"splashpos=m,m\0" \
	"console=" CONSOLE_DEV "\0" \
	"earlycon=" EARLY_CONSOLE "\0" \
	"fdt_addr=0x43000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"boot_fdt=try\0" \
	"ip_dyn=yes\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"initrd_addr=0x43800000\0"		\
	"initrd_high=0xffffffffffffffff\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"mmcbootpart=" __stringify(EMMC_BOOT_PART) "\0" \
	"mmcdev="__stringify(EMMC_BOOT_DEV)"\0" \
	"mmcpart=1\0" \
	"mmcroot=PARTUUID=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"bootargs_tftp=" \
		"if test ${ip_dyn} = yes; then " \
			"bootargs_ip=\"ip=dhcp\";" \
		"else " \
			"bootargs_ip=\"ip=\\${ipaddr}:\\${serverip}:" \
			"\\${gatewayip}:\\${netmask}:\\${hostname}:" \
			"eth0:off\";" \
		"fi;\0" \
	"bootargs_mmc_android=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_android} ${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_mmc_linux=setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=${mmcroot} rootwait rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_tftp_linux=run bootargs_tftp;" \
		"setenv bootargs console=${console},${baudrate} " \
		"${bootargs_linux} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_nfs_linux=run bootargs_tftp_linux\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} root=${mmcroot} " \
	"loadbootscript=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"loadimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=load mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"partition_mmc_android=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"gpt write mmc ${mmcdev} ${parts_android};" \
			"mmc rescan;" \
		"fi;\0" \
	"partition_mmc_linux=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"if test \"${dualboot}\" = yes; then " \
				"gpt write mmc ${mmcdev} ${parts_linux_dualboot};" \
			"else " \
				"gpt write mmc ${mmcdev} ${parts_linux};" \
			"fi;" \
			"mmc rescan;" \
		"fi;\0" \
	"recoverycmd=setenv mmcpart " RECOVERY_PARTITION ";" \
		"boot\0" \
	"recovery_file=recovery.img\0" \
	"linux_file=dey-image-qt-xwayland-" BOARD_DEY_NAME ".boot.vfat\0" \
	"rootfs_file=dey-image-qt-xwayland-" BOARD_DEY_NAME ".ext4\0" \
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
	"bootcmd_mfg=fastboot " __stringify(CONFIG_FASTBOOT_USB_DEV) "\0" \
	"active_system=linux_a\0" \
	"usb_pgood_delay=2000\0" \
	""	/* end line */

#undef CONFIG_BOOTCOMMAND
#ifdef CONFIG_SECURE_BOOT
/*
 * Authenticate bootscript before running it. IVT offset is at
 * ${filesize} - CONFIG_CSF_SIZE - IVT_SIZE (0x20)
 * Use 0x2000 as CSF_SIZE, as this is the value used by the script
 * to sign / encrypt the bootscript
 */
#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"setexpr bs_ivt_offset ${filesize} - 0x2020;" \
		"if hab_auth_img ${loadaddr} ${filesize} ${bs_ivt_offset}; then " \
			"source ${loadaddr};" \
		"fi; " \
	"fi;"
#else
#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"source ${loadaddr};" \
	"fi;"

#endif	/* CONFIG_SECURE_BOOT */

/* Android specific configuration */
#if defined(CONFIG_ANDROID_SUPPORT)
#include "ccimx8mm_dvk_android.h"
#endif

#endif /* __CCIMX8MM_DVK_H */
