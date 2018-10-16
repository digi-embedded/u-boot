/*
 * Copyright 2017 NXP
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_SBC_PRO_CONFIG_H
#define CCIMX8X_SBC_PRO_CONFIG_H

#include "ccimx8x_common.h"

#define CONFIG_BOARD_DESCRIPTION	"SBC Pro"
#define BOARD_DEY_NAME			"ccimx8x-sbc-pro"

#define CONFIG_REMAKE_ELF

/* GPIO configs */
#define CONFIG_MXC_GPIO

/* ENET Config */
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define FEC_QUIRK_ENET_MAC
#define CONFIG_PHY_ATHEROS
#define CONFIG_PHY_GIGE                 /* Support for 1000BASE-X */

#define CONFIG_FEC_XCV_TYPE             RGMII
#define CONFIG_FEC_ENET_DEV             0
#if (CONFIG_FEC_ENET_DEV == 0)
#define IMX_FEC_BASE                    0x5B040000
#define CONFIG_ETHPRIME                 "eth0"
#elif (CONFIG_FEC_ENET_DEV == 1)
#define IMX_FEC_BASE                    0x5B050000
#define CONFIG_ETHPRIME                 "eth1"
#endif

/* ENET0 MDIO are shared */
#define CONFIG_FEC_MXC_MDIO_BASE	0x5B040000

/* Serial */
#define CONSOLE_DEV			"ttyLP2"
#define EARLY_CONSOLE			"lpuart32,0x5a080000"
#define CONFIG_BAUDRATE			115200

/* USB Config */
#ifdef CONFIG_CMD_USB
#define CONFIG_USB_MAX_CONTROLLER_COUNT 2

/* USB 3.0 controller configs */
#ifdef CONFIG_USB_XHCI_IMX8
#define CONFIG_SYS_USB_XHCI_MAX_ROOT_PORTS	2
#endif

/* USB OTG controller configs */
#ifdef CONFIG_USB_EHCI_HCD
#define CONFIG_USB_HOST_ETHER
#define CONFIG_USB_ETHER_ASIX
#define CONFIG_MXC_USB_PORTSC		(PORT_PTS_UTMI | PORT_PTS_PTW)
#endif
#endif /* CONFIG_CMD_USB */

#ifdef CONFIG_USB_GADGET
#define CONFIG_USBD_HS
#define CONFIG_USB_FUNCTION_MASS_STORAGE
#endif

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CONFIG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} " \
		"root=/dev/ram0 rw ramdisk_size=524288 quiet " \
		"\0" \
	"bootcmd_mfg=source ${loadaddr}\0" \

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	CONFIG_MFG_ENV_SETTINGS \
	CONFIG_DEFAULT_NETWORK_SETTINGS		\
	CONFIG_EXTRA_NETWORK_SETTINGS		\
	RANDOM_UUIDS \
	"dboot_kernel_var=imagegz\0" \
	"lzipaddr=0x82000000\0" \
	"script=boot.scr\0" \
	"loadscript=load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"image=Image-" BOARD_DEY_NAME ".bin\0" \
	"imagegz=Image.gz-" BOARD_DEY_NAME ".bin\0" \
	"uboot_file=imx-boot-" BOARD_DEY_NAME ".bin\0" \
	"panel=NULL\0" \
	"console=" CONSOLE_DEV "\0" \
	"earlycon=" EARLY_CONSOLE "\0" \
	"fdt_addr=0x83000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"boot_fdt=try\0" \
	"ip_dyn=yes\0" \
	"fdt_file=Image.gz-" BOARD_DEY_NAME ".dtb\0" \
	"initrd_addr=0x83800000\0"		\
	"initrd_high=0xffffffffffffffff\0" \
	"mmcbootpart=" __stringify(CONFIG_SYS_BOOT_PART_EMMC) "\0" \
	"mmcdev="__stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=" __stringify(CONFIG_SYS_MMC_IMG_LOAD_PART) "\0" \
	"mmcroot=PARTUUID=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"bootargs_tftp=" \
		"if test ${ip_dyn} = yes; then " \
			"bootargs_ip=\"ip=dhcp\";" \
		"else " \
			"bootargs_ip=\"ip=\\${ipaddr}:\\${serverip}:" \
			"\\${gatewayip}:\\${netmask}:\\${hostname}:" \
			"eth0:off\";" \
		"fi;\0" \
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
	"video=imxdpufb5:off video=imxdpufb6:off video=imxdpufb7:off\0" \
	"loadbootscript=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"loadimage=load mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=load mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"partition_mmc_linux=mmc rescan;" \
		"if mmc dev ${mmcdev} 0; then " \
			"gpt write mmc ${mmcdev} ${parts_linux};" \
			"mmc rescan;" \
		"else " \
			"if mmc dev ${mmcdev};then " \
				"gpt write mmc ${mmcdev} ${parts_linux};" \
				"mmc rescan;" \
			"else;" \
			"fi;" \
		"fi;\0" \
	"recoverycmd=setenv mmcpart " CONFIG_RECOVERY_PARTITION ";" \
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
	""	/* end line */

#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"source ${loadaddr};" \
	"fi;"

#endif /* CCIMX8X_SBC_PRO_CONFIG_H */
