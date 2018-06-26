/*
 * Copyright 2017 NXP
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef CCIMX8X_SBC_EXPRESS_CONFIG_H
#define CCIMX8X_SBC_EXPRESS_CONFIG_H

#include "ccimx8x_common.h"

#define CONFIG_BOARD_DESCRIPTION	"SBC Express"

#define CONFIG_REMAKE_ELF

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_ARCH_MISC_INIT

/* GPIO configs */
#define CONFIG_MXC_GPIO

/* ENET Config */
#define CONFIG_FEC_MXC
#define CONFIG_MII
#define FEC_QUIRK_ENET_MAC
#define CONFIG_RESET_PHY_R
#define CONFIG_PHY_SMSC

#define CONFIG_FEC_ENET_DEV 0
#if (CONFIG_FEC_ENET_DEV == 0)
#define IMX_FEC_BASE			0x5B040000
#define CONFIG_FEC_MXC_PHYADDR          0x0
#define CONFIG_ETHPRIME                 "FEC0"
#define CONFIG_FEC_XCV_TYPE             RMII
#endif

/* ENET0 MDIO are shared */
#define CONFIG_FEC_MXC_MDIO_BASE	0x5B040000

/* protected environment variables (besides ethaddr and serial#) */
#define CONFIG_ENV_FLAGS_LIST_STATIC	\
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

/* Boot M4 */
#define M4_BOOT_ENV \
	"m4_0_image=m4_0.bin\0" \
	"loadm4image_0=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${m4_0_image}\0" \
	"m4boot_0=run loadm4image_0; dcache flush; bootaux ${loadaddr} 0\0" \

#ifdef CONFIG_NAND_BOOT
#define MFG_NAND_PARTITION "mtdparts=gpmi-nand:64m(boot),16m(kernel),16m(dtb),1m(misc),-(rootfs) "
#else
#define MFG_NAND_PARTITION ""
#endif

#define CONFIG_MFG_ENV_SETTINGS \
	"mfgtool_args=setenv bootargs console=${console},${baudrate} " \
		"rdinit=/linuxrc " \
		"g_mass_storage.stall=0 g_mass_storage.removable=1 " \
		"g_mass_storage.idVendor=0x066F g_mass_storage.idProduct=0x37FF "\
		"g_mass_storage.iSerialNumber=\"\" "\
		MFG_NAND_PARTITION \
		"video=imxdpufb5:off video=imxdpufb6:off video=imxdpufb7:off "\
		"clk_ignore_unused "\
		"\0" \
	"initrd_addr=0x83800000\0" \
	"initrd_high=0xffffffff\0" \
	"bootcmd_mfg=run mfgtool_args;booti ${loadaddr} ${initrd_addr} ${fdt_addr};\0" \

#define XEN_ENV \
	"xen_addr=0x80200000\0" \
	"xen_file=xen\0" \
	"xenargs=setenv bootargs console=dtuart dtuart=/serial@5a060000 dom0_mem=1024M \0" \
	"loadxen=fatload mmc ${mmcdev}:${mmcpart} ${xen_addr} ${xen_file}\0" \
	"xenboot=setenv loadaddr 0x80a00000; setenv fdt_file fsl-imx8qxp-mek-dom0.dtb; "\
	"setenv bootargs console=dtuart dtuart=/serial@5a060000 dom0_mem=1024M; " \
	"run loadfdt; run loadxen; run loadimage; fdt addr ${fdt_addr}; "\
	"fdt set /chosen/module@0 reg <0x00000000 ${loadaddr} 0x00000000 0x${filesize}>; " \
	"booti ${xen_addr} - ${fdt_addr} \0" \

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS		\
	CONFIG_MFG_ENV_SETTINGS \
	CONFIG_DEFAULT_NETWORK_SETTINGS		\
	RANDOM_UUIDS \
	M4_BOOT_ENV \
	XEN_ENV \
	"dboot_kernel_var=imagegz\0" \
	"lzipaddr=0x82000000\0" \
	"script=boot.scr\0" \
	"loadscript=load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}\0" \
	"image=Image-ccimx8x-sbc-express.bin\0" \
	"imagegz=Image-ccimx8x-sbc-express.gz\0" \
	"uboot_file=u-boot-ccimx8x-sbc-express.bin\0" \
	"panel=NULL\0" \
	"console=" CONSOLE_DEV "\0" \
	"earlycon=" EARLY_CONSOLE "\0" \
	"fdt_addr=0x83000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"boot_fdt=try\0" \
	"ip_dyn=yes\0" \
	"fdt_file=Image-" CONFIG_DEFAULT_DEVICE_TREE ".dtb\0" \
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
		"earlycon=${earlycon},${baudrate} " \
		"${bootargs_linux} root=${mmcroot} rootwait rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_tftp_linux=run bootargs_tftp;" \
		"setenv bootargs console=${console},${baudrate} " \
		"earlycon=${earlycon},${baudrate} " \
		"${bootargs_linux} root=/dev/nfs " \
		"${bootargs_ip} nfsroot=${serverip}:${rootpath},v3,tcp " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"bootargs_nfs_linux=run bootargs_tftp_linux\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs console=${console},${baudrate} root=${mmcroot} " \
	"video=imxdpufb5:off video=imxdpufb6:off video=imxdpufb7:off\0" \
	"loadbootscript=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${script};\0" \
	"loadimage=fatload mmc ${mmcdev}:${mmcpart} ${loadaddr} ${image}\0" \
	"loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr} ${fdt_file}\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"if run loadfdt; then " \
				"booti ${loadaddr} - ${fdt_addr}; " \
			"else " \
				"echo WARN: Cannot load the DT; " \
			"fi; " \
		"else " \
			"echo wait for boot; " \
		"fi;\0" \
	"netargs=setenv bootargs console=${console} " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp " \
		"video=imxdpufb5:off video=imxdpufb6:off video=imxdpufb7:off\0" \
	"netboot=echo Booting from net ...; " \
		"run netargs;  " \
		"if test ${ip_dyn} = yes; then " \
			"setenv get_cmd dhcp; " \
		"else " \
			"setenv get_cmd tftp; " \
		"fi; " \
		"${get_cmd} ${loadaddr} ${image}; " \
		"if test ${boot_fdt} = yes || test ${boot_fdt} = try; then " \
			"if ${get_cmd} ${fdt_addr} ${fdt_file}; then " \
				"booti ${loadaddr} - ${fdt_addr}; " \
			"else " \
				"echo WARN: Cannot load the DT; " \
			"fi; " \
		"else " \
			"booti; " \
		"fi;\0" \
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
		"fi;\0"

#define CONFIG_BOOTCOMMAND \
	"if run loadscript; then " \
		"source ${loadaddr};" \
	"fi;"

/* Serial */
#define CONSOLE_DEV			"ttyLP2"
#define EARLY_CONSOLE			"lpuart32,0x5a080000"
#define CONFIG_BAUDRATE			115200

/* Generic Timer Definitions */
#define COUNTER_FREQUENCY		8000000	/* 8MHz */

#define CONFIG_IMX_SMMU

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

#endif /* CCIMX8X_SBC_EXPRESS_CONFIG_H */
