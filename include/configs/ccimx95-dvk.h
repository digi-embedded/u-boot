/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2023 NXP
 * Copyright 2025 Digi International Inc.
 */

#ifndef CCIMX95_DVK_H
#define CCIMX95_DVK_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>
#include <configs/ccimx9_common.h>
#include "imx_env.h"

#define CONFIG_SOM_DESCRIPTION		"ConnectCore 95"
#define CONFIG_BOARD_DESCRIPTION	"Development Kit"
#define BOARD_DEY_NAME			"ccimx95-dvk"

/* Carrier board version in environment */
#define CONFIG_HAS_CARRIERBOARD_VERSION
#define CONFIG_HAS_CARRIERBOARD_ID

#define CFG_SYS_UBOOT_BASE	\
	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_AHAB_BOOT
#define AHAB_ENV "sec_boot=yes\0"
#else
#define AHAB_ENV "sec_boot=no\0"
#endif

#ifdef CONFIG_DISTRO_DEFAULTS
#define BOOT_TARGET_DEVICES(func) \
	func(MMC, mmc, 0) \
	func(MMC, mmc, 1) \
	func(USB, usb, 0)

#include <config_distro_bootcmd.h>
#else
#define BOOTENV
#endif

#ifdef CONFIG_TARGET_CCIMX95_DVK
#define JH_ROOT_DTB "ccimx95-dvk-root.dtb"
/* jh_root_mem: set the memory space used by Jailhouse root cell */
#define JAILHOUSE_ENV \
	"jh_root_dtb=" JH_ROOT_DTB "\0" \
	"jh_mmcboot=setenv fdt_file ${jh_root_dtb}; " \
		"setenv jh_clk kvm.enable_virt_at_load=false cpuidle.off=1 clk_ignore_unused kvm-arm.mode=nvhe; " \
		"setenv jh_root_mem 0x58000000@0x90000000,0xc0000000@0x180000000; " \
		"if run loadimage; then run mmcboot;" \
		"else run jh_netboot; fi; \0" \
	"jh_netboot=setenv fdt_file ${jh_root_dtb}; " \
		"setenv jh_root_mem 0x58000000@0x90000000,0xc0000000@0x180000000; " \
		"setenv jh_clk kvm.enable_virt_at_load=false cpuidle.off=1 clk_ignore_unused kvm-arm.mode=nvhe; run netboot; \0 "
#else
#define JH_ROOT_DTB "ccimx95-dvk-root.dtb"
/* jh_root_mem: set the memory space used by Jailhouse root cell */
 #define JAILHOUSE_ENV \
	"jh_root_dtb=" JH_ROOT_DTB "\0" \
	"jh_mmcboot=setenv fdt_file ${jh_root_dtb}; " \
		"setenv jh_clk kvm.enable_virt_at_load=false cpuidle.off=1 clk_ignore_unused kvm-arm.mode=nvhe; " \
		"setenv jh_root_mem 0x58000000@0x90000000,0x300000000@0x180000000; " \
		"if run loadimage; then run mmcboot;" \
		"else run jh_netboot; fi; \0" \
	"jh_netboot=setenv fdt_file ${jh_root_dtb}; " \
		"setenv jh_root_mem 0x58000000@0x90000000,0x300000000@0x180000000; " \
		"setenv jh_clk kvm.enable_virt_at_load=false cpuidle.off=1 clk_ignore_unused kvm-arm.mode=nvhe; run netboot; \0 "
#endif

#define CFG_MFG_ENV_SETTINGS \
	CFG_MFG_ENV_SETTINGS_DEFAULT \
	"fastboot_dev=mmc" __stringify(EMMC_BOOT_DEV) "\0" \
	"initrd_addr=0x93800000\0" \
	"initrd_high=0xffffffffffffffff\0" \
	"emmc_dev=0\0"\
	"sd_dev=1\0" \

#define DUALBOOT_ENV_SETTINGS \
	"active_system=linux_a\0"

#define XEN_BOOT_ENV \
	    "domu-android-auto=no\0" \
            "xenhyper_bootargs=console=dtuart dom0_mem=4096M dom0_max_vcpus=2 pci-passthrough=on\0" \
            "xenlinux_bootargs= \0" \
            "xenlinux_console=hvc0 earlycon=xen\0" \
            "xenlinux_addr=0x9c000000\0" \
            "dom0fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
            "xenboot_common=" \
                "${get_cmd} ${loadaddr} xen;" \
                "${get_cmd} ${fdt_addr} ${dom0fdt_file};" \
                "${get_cmd} ${xenlinux_addr} ${image};" \
                "fdt addr ${fdt_addr};" \
                "fdt resize 256;" \
                "fdt mknode /chosen module@0;" \
                "fdt set /chosen/module@0 reg <0x00000000 ${xenlinux_addr} 0x00000000 0x${filesize}>;" \
                "fdt set /chosen/module@0 bootargs \"${bootargs} ${xenlinux_bootargs}\"; " \
                "fdt set /soc/bus@49000000/iommu@490d0000 status disabled;" \
		"fdt set /chosen/module@0 compatible \"xen,linux-zimage\" \"xen,multiboot-module\";" \
                "setenv bootargs ${xenhyper_bootargs};" \
                "booti ${loadaddr} - ${fdt_addr};" \
            "\0" \
            "xennetboot=" \
                "setenv get_cmd dhcp;" \
                "setenv console ${xenlinux_console};" \
		"setenv jh_clk clk_ignore_unused;" \
                "run netargs;" \
                "run xenboot_common;" \
            "\0" \
            "xenmmcboot=" \
                "setenv get_cmd \"fatload mmc ${mmcdev}:${mmcpart}\";" \
                "setenv console ${xenlinux_console};" \
		"setenv jh_clk clk_ignore_unused;" \
                "run mmcargs;" \
                "run xenboot_common;" \
            "\0" \
/* Initial environment variables */
#define CFG_EXTRA_ENV_SETTINGS		\
	JAILHOUSE_ENV \
	CFG_MFG_ENV_SETTINGS \
	DUALBOOT_ENV_SETTINGS \
	XEN_BOOT_ENV \
	BOOTENV \
	AHAB_ENV \
	"prepare_mcore=setenv mcore_args pd_ignore_unused;\0" \
	CONFIG_DEFAULT_NETWORK_SETTINGS \
	CONFIG_EXTRA_NETWORK_SETTINGS \
	RANDOM_UUIDS \
	ALTBOOTCMD \
	"dualboot=yes\0" \
	"cpuidle= \0" \
	"scriptaddr=0x93500000\0" \
	"kernel_addr_r=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"dboot_kernel_var=imagegz\0" \
	"lzipaddr=" __stringify(CONFIG_DIGI_LZIPADDR) "\0" \
	"fitimage=fitImage-" BOARD_DEY_NAME ".bin\0" \
	"image=Image-" BOARD_DEY_NAME ".bin\0" \
	"imagegz=Image.gz-" BOARD_DEY_NAME ".bin\0" \
	"uboot_file=imx-boot-" BOARD_DEY_NAME ".bin\0" \
	"splashimage=0xA0000000\0" \
	"console=ttyLP5,115200 earlycon\0" \
	"fdt_addr_r=0x93000000\0"			\
	"fdt_addr=0x93000000\0"			\
	"fdt_high=0xffffffffffffffff\0"		\
	"fit_addr_r=" __stringify(CONFIG_DIGI_LZIPADDR) "\0" \
	"cntr_addr=0xA8000000\0"			\
	"cntr_file=os_cntr_signed.bin\0" \
	"boot_fit=no\0" \
	"fdt_file=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"bootm_size=0x10000000\0" \
	"mmcbootpart=" __stringify(EMMC_BOOT_PART) "\0" \
	"mmcdev=" __stringify(CONFIG_SYS_MMC_ENV_DEV)"\0" \
	"mmcpart=1\0" \
	"mmcroot=PARTUUID=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"mmcautodetect=yes\0" \
	"mmcargs=setenv bootargs ${cpuidle} ${jh_clk} ${mcore_args} console=${console} root=${mmcroot}\0 " \
	"loadbootscript=" \
		"if test \"${dualboot}\" = yes; then " \
			"env exists active_system || setenv active_system linux_a; " \
			"part number mmc ${mmcbootdev} ${active_system} mmcpart; " \
		"fi;" \
		"if test \"${dboot_kernel_var}\" = fitimage; then " \
			"load mmc ${mmcbootdev}:${mmcpart} ${fit_addr_r} ${fitimage}; " \
			"env set source_fit_script ${fit_addr_r}:${fit-script}; " \
		"else " \
			"load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${script}; " \
		"fi;\0" \
	"loadimage=" \
		"if test \"${dualboot}\" = yes; then " \
			"env exists active_system || setenv active_system linux_a; " \
			"part number mmc ${mmcbootdev} ${active_system} mmcpart; " \
		"fi;" \
		"if test \"${dboot_kernel_var}\" = fitimage; then " \
			"load mmc ${mmcbootdev}:${mmcpart} ${fit_addr_r} ${fitimage}; " \
		"else " \
			"load mmc ${mmcbootdev}:${mmcpart} ${loadaddr} ${image}; " \
		"fi;\0" \
	"loadfdt=fatload mmc ${mmcdev}:${mmcpart} ${fdt_addr_r} ${fdt_file}\0" \
	"loadcntr=fatload mmc ${mmcdev}:${mmcpart} ${cntr_addr} ${cntr_file}\0" \
	"auth_os=booti ${cntr_addr}\0" \
	"boot_os=booti ${loadaddr} - ${fdt_addr_r};\0" \
	"bootargs_linux=fbcon=logo-pos:center fbcon=logo-count:1\0" \
	"bootargs_mmc_linux=setenv bootargs console=${console} " \
		"${bootargs_linux} root=${mmcroot} rootwait rw " \
		"${bootargs_once} ${extra_bootargs}\0" \
	"mmcboot=echo Booting from mmc ...; " \
		"run mmcargs; " \
		"if test ${sec_boot} = yes; then " \
			"run auth_os; " \
		"else " \
			"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
				"bootm ${loadaddr}; " \
			"else " \
				"if run loadfdt; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi;" \
		"fi;\0" \
	"netargs=setenv bootargs ${cpuidle} ${jh_clk} ${mcore_args} console=${console} " \
		"root=/dev/nfs " \
		"ip=dhcp nfsroot=${serverip}:${nfsroot},v3,tcp\0" \
	"netboot=echo Booting from net ...; " \
		"run netargs;  " \
		"if test ${ip_dyn} = yes; then " \
			"setenv get_cmd dhcp; " \
		"else " \
			"setenv get_cmd tftp; " \
		"fi; " \
		"if test ${sec_boot} = yes; then " \
			"${get_cmd} ${cntr_addr} ${cntr_file}; " \
			"run auth_os; " \
		"else " \
			"${get_cmd} ${loadaddr} ${image}; " \
			"if test ${boot_fit} = yes || test ${boot_fit} = try; then " \
				"bootm ${loadaddr}; " \
			"else " \
				"if ${get_cmd} ${fdt_addr_r} ${fdt_file}; then " \
					"run boot_os; " \
				"else " \
					"echo WARN: Cannot load the DT; " \
				"fi; " \
			"fi;" \
		"fi;\0" \
	"parts_linux_dualboot=" LINUX_DUALBOOT_16GB_PARTITION_TABLE "\0" \
	"parts_linux=" LINUX_16GB_PARTITION_TABLE "\0" \
	"partition_mmc_linux=mmc rescan;" \
		"if mmc dev ${mmcdev}; then " \
			"if test \"${dualboot}\" = yes; then " \
				"gpt write mmc ${mmcdev} ${parts_linux_dualboot};" \
			"else " \
				"gpt write mmc ${mmcdev} ${parts_linux};" \
			"fi;" \
			"mmc rescan;" \
		"fi;\0" \
	"install_linux_fw_sd=if load mmc 1 ${loadaddr} install_linux_fw_sd.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"install_linux_fw_usb=usb start;" \
		"if load usb 0 ${loadaddr} install_linux_fw_usb.scr;then " \
			"source ${loadaddr};" \
		"fi;\0" \
	"update_addr=" __stringify(CONFIG_DIGI_UPDATE_ADDR) "\0" \
	"recoverycmd=setenv mmcpart " RECOVERY_PARTITION ";" \
		"boot\0" \
	"script=boot.scr\0" \
	"fit-script=bootscr-boot.txt\0" \
	"bsp_bootcmd=echo Running BSP bootcmd ...; " \
		"mmc dev ${mmcdev}; if mmc rescan; then " \
		   "if run loadbootscript; then " \
			   "echo Running bootscript from mmc ...; " \
			   "source ${source_fit_script}; " \
		   "else " \
			   "if test ${sec_boot} = yes; then " \
				   "if run loadcntr; then " \
					   "run mmcboot; " \
				   "else run netboot; " \
				   "fi; " \
			    "else " \
				   "if run loadimage; then " \
					   "run mmcboot; " \
				   "else run netboot; " \
				   "fi; " \
				"fi; " \
		   "fi; " \
	   "fi;"

/* Link Definitions */

/*
 * The first 256MB of DRAM (0x80000000 to 0x8fffffff) are located below the
 * u-boot base address. Optee currently allocates 32MB for its core and 2MB
 * for shared memory within the 0x8c000000 to 0x8e200000 range.
 */
#define CFG_SYS_INIT_RAM_ADDR        0x90000000
#define CFG_SYS_INIT_RAM_SIZE        0x200000

#define CFG_SYS_SDRAM_BASE           0x90000000
#define PHYS_SDRAM                      0x90000000
/* Totally 8GB */
#define PHYS_SDRAM_SIZE			0x70000000UL /* 2GB  - 256MB DDR */
#define PHYS_SDRAM_2_SIZE 		0x180000000UL /* 6GB */

#define CFG_SYS_FSL_USDHC_NUM	2

/* Using ULP WDOG for reset */
#define WDOG_BASE_ADDR          WDG3_BASE_ADDR

/* USB configs */
#if defined(CONFIG_CMD_NET)
#define PHY_ANEG_TIMEOUT 20000
/* Number of Rx BD rings: 8 per ENETC instance */
#endif

#ifdef CONFIG_ANDROID_SUPPORT
#include "imx95_evk_android.h"
#endif

#endif
