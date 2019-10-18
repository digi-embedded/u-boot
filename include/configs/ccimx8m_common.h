/*
 * Copyright 2019 Digi International Inc
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CCIMX8M_COMMON_H
#define __CCIMX8M_COMMON_H

#include <asm/arch/imx-regs.h>
#include "digi_common.h"		/* Load Digi common stuff... */

#define CONFIG_CC8
#define CONFIG_CC8M

#ifdef CONFIG_SECURE_BOOT
#define CONFIG_CSF_SIZE			0x2000 /* 8K region */
#endif

#define CONFIG_SPL_MAX_SIZE		(148 * 1024)
#define CONFIG_SYS_MONITOR_LEN		(512 * 1024)
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_USE_SECTOR
#define CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR	0x300
#define CONFIG_SYS_MMCSD_FS_BOOT_PARTITION	1
#define CONFIG_SYS_UBOOT_BASE		(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#ifdef CONFIG_SPL_BUILD
/*#define CONFIG_ENABLE_DDR_TRAINING_DEBUG*/
#define CONFIG_SPL_WATCHDOG_SUPPORT
#define CONFIG_SPL_POWER_SUPPORT
#define CONFIG_SPL_DRIVERS_MISC_SUPPORT
#define CONFIG_SPL_I2C_SUPPORT
#define CONFIG_SPL_LDSCRIPT		"arch/arm/cpu/armv8/u-boot-spl.lds"
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_GPIO_SUPPORT
#define CONFIG_SYS_ICACHE_OFF
#define CONFIG_SYS_DCACHE_OFF

#define CONFIG_SPL_ABORT_ON_RAW_IMAGE /* For RAW image gives a error info not panic */

#define CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG

#endif

#define CONFIG_CMD_READ
#define CONFIG_SERIAL_TAG
#define CONFIG_FASTBOOT_USB_DEV 0

#define CONFIG_REMAKE_ELF

#define CONFIG_BOARD_EARLY_INIT_F
#define CONFIG_BOARD_POSTCLK_INIT
#define CONFIG_BOARD_LATE_INIT

#undef CONFIG_CMD_EXPORTENV
#undef CONFIG_CMD_IMPORTENV
#undef CONFIG_CMD_IMLS

#undef CONFIG_CMD_CRC32
#undef CONFIG_BOOTM_NETBSD

/* HWID */
#define CONFIG_HAS_HWID
#define CONFIG_HWID_BANK		9
#define CONFIG_HWID_START_WORD		0
#define CONFIG_HWID_WORDS_NUMBER	3
#define CONFIG_HWID_LOCK_FUSE		(0x3 << 14)
#define CONFIG_CMD_FUSE

#define OCOTP_LOCK_BANK		0
#define OCOTP_LOCK_WORD		0

/* Link Definitions */
#define CONFIG_LOADADDR			0x40480000

#define CONFIG_SYS_LOAD_ADDR           CONFIG_LOADADDR

#define CONFIG_SYS_INIT_RAM_ADDR        0x40000000
#define CONFIG_SYS_INIT_RAM_SIZE        0x80000
#define CONFIG_SYS_INIT_SP_OFFSET \
        (CONFIG_SYS_INIT_RAM_SIZE - GENERATED_GBL_DATA_SIZE)
#define CONFIG_SYS_INIT_SP_ADDR \
        (CONFIG_SYS_INIT_RAM_ADDR + CONFIG_SYS_INIT_SP_OFFSET)

#define CONFIG_ENV_OVERWRITE
#define CONFIG_ENV_OFFSET		(1792 * 1024)	/* 256kB below 2MiB */
#define CONFIG_ENV_OFFSET_REDUND	(CONFIG_ENV_OFFSET + (128 * 1024))
#define CONFIG_ENV_SIZE_REDUND		CONFIG_ENV_SIZE

#define CONFIG_SYS_STORAGE_MEDIA       "mmc"
#define CONFIG_SYS_MMC_ENV_DEV		1   /* USDHC3 */
#define CONFIG_SYS_MMC_ENV_PART		2   /* Boot2 partition of eMMC */

/* Size of malloc() pool */
#define CONFIG_SYS_MALLOC_LEN		((CONFIG_ENV_SIZE + (2*1024) + (16*1024)) * 1024)

#define CONFIG_SYS_SDRAM_BASE           0x40000000
#define PHYS_SDRAM                      0x40000000

#define CONFIG_SYS_MEMTEST_START    PHYS_SDRAM
#define CONFIG_SYS_MEMTEST_END      (CONFIG_SYS_MEMTEST_START + (PHYS_SDRAM_SIZE >> 1))

/* Monitor Command Prompt */
#define CONFIG_SYS_PROMPT_HUSH_PS2     "> "
#define CONFIG_SYS_CBSIZE              2048

#define CONFIG_IMX_BOOTAUX

/* USDHC */
#define CONFIG_FSL_ESDHC
#define CONFIG_FSL_USDHC

#define CONFIG_SUPPORT_EMMC_BOOT	/* eMMC specific */
#define CONFIG_SYS_MMC_IMG_LOAD_PART	1

#define CONFIG_MXC_GPIO

#define CONFIG_MXC_OCOTP
#define CONFIG_CMD_FUSE

#ifndef CONFIG_DM_I2C
#define CONFIG_SYS_I2C
#endif
#define CONFIG_SYS_I2C_MXC_I2C1		/* enable I2C bus 1 */
#define CONFIG_SYS_I2C_MXC_I2C2		/* enable I2C bus 2 */
#define CONFIG_SYS_I2C_MXC_I2C3		/* enable I2C bus 3 */
#define CONFIG_SYS_I2C_SPEED		100000

/* USB configs */
#ifndef CONFIG_SPL_BUILD
#define CONFIG_USBD_HS
#endif

#define CONFIG_USB_GADGET_DUALSPEED
#define CONFIG_USB_GADGET_VBUS_DRAW 2

#define CONFIG_CI_UDC

#define CONFIG_MXC_USB_PORTSC  (PORT_PTS_UTMI | PORT_PTS_PTW)
#define CONFIG_USB_MAX_CONTROLLER_COUNT         2

#ifdef CONFIG_VIDEO
#define CONFIG_VIDEO_MXS
#define CONFIG_VIDEO_LOGO
#define CONFIG_SPLASH_SCREEN
#define CONFIG_SPLASH_SCREEN_ALIGN
#define CONFIG_CMD_BMP
#define CONFIG_BMP_16BPP
#define CONFIG_VIDEO_BMP_RLE8
#define CONFIG_VIDEO_BMP_LOGO
#define CONFIG_IMX_VIDEO_SKIP
#define CONFIG_RM67191
#endif

#define CONFIG_OF_SYSTEM_SETUP

/* Pool of randomly generated UUIDs at host machine */
#define RANDOM_UUIDS	\
	"uuid_disk=075e2a9b-6af6-448c-a52a-3a6e69f0afff\0" \
	"part1_uuid=43f1961b-ce4c-4e6c-8f22-2230c5d532bd\0" \
	"part2_uuid=f241b915-4241-47fd-b4de-ab5af832a0f6\0" \
	"part3_uuid=1c606ef5-f1ac-43b9-9bb5-d5c578580b6b\0" \
	"part4_uuid=c7d8648b-76f7-4e2b-b829-e95a83cc7b32\0" \
	"part5_uuid=ebae5694-6e56-497c-83c6-c4455e12d727\0" \
	"part6_uuid=3845c9fc-e581-49f3-999f-86c9bab515ef\0" \
	"part7_uuid=3fcf7bf1-b6fe-419d-9a14-f87950727bc0\0" \
	"part8_uuid=12c08a28-fb40-430a-a5bc-7b4f015b0b3c\0" \
	"part9_uuid=dc83dea8-c467-45dc-84eb-5e913daec17e\0" \
	"part10_uuid=df0dba76-d5e0-11e8-9f8b-f2801f1b9fd1\0"

#define LINUX_4GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=1536MiB,uuid=${part3_uuid};" \
	"name=update,size=1536MiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_8GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=3GiB,uuid=${part3_uuid};" \
	"name=update,size=3GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define LINUX_16GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=linux,size=64MiB,uuid=${part1_uuid};" \
	"name=recovery,size=64MiB,uuid=${part2_uuid};" \
	"name=rootfs,size=7GiB,uuid=${part3_uuid};" \
	"name=update,size=7GiB,uuid=${part4_uuid};" \
	"name=safe,size=16MiB,uuid=${part5_uuid};" \
	"name=safe2,size=16MiB,uuid=${part6_uuid};" \
	"name=data,size=-,uuid=${part7_uuid};" \
	"\""

#define ANDROID_4GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=1024MiB,uuid=${part3_uuid};" \
	"name=cache,size=1024MiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

#define ANDROID_8GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=2GiB,uuid=${part3_uuid};" \
	"name=cache,size=2GiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

#define ANDROID_16GB_PARTITION_TABLE \
	"\"uuid_disk=${uuid_disk};" \
	"start=2MiB," \
	"name=boot,size=32MiB,uuid=${part1_uuid};" \
	"name=recovery,size=32MiB,uuid=${part2_uuid};" \
	"name=system,size=2GiB,uuid=${part3_uuid};" \
	"name=cache,size=2GiB,uuid=${part4_uuid};" \
	"name=vendor,size=112MiB,uuid=${part5_uuid};" \
	"name=datafooter,size=16MiB,uuid=${part6_uuid};" \
	"name=safe,size=16MiB,uuid=${part7_uuid};" \
	"name=frp,size=1MiB,uuid=${part8_uuid};" \
	"name=metadata,size=16MiB,uuid=${part9_uuid};" \
	"name=userdata,size=-,uuid=${part10_uuid};" \
	"\""

/* Partition defines */
#define CONFIG_RECOVERY_PARTITION	"2"

#endif /* __CCIMX8M_COMMON_H */
