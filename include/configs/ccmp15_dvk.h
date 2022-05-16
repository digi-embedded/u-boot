/* SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause */
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 *
 * Configuration settings for the STM32MP15x CPU
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include <configs/ccmp1_common.h>

#ifdef CONFIG_FASTBOOT_CMD_OEM_FORMAT
/* eMMC default partitions for fastboot command: oem format */
#define PARTS_DEFAULT \
	"partitions=" \
	"name=ssbl,size=2M;" \
	"name=bootfs,size=64MB,bootable;" \
	"name=vendorfs,size=16M;" \
	"name=rootfs,size=746M;" \
	"name=userfs,size=-\0"
#else
#define PARTS_DEFAULT
#endif

/*
 * memory layout for 32M uncompressed/compressed kernel,
 * 1M fdt, 1M script, 1M pxe and 1M for splashimage
 * and the ramdisk at the end.
 */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_addr_r=0xc2000000\0" \
	"fdt_addr_r=0xc4000000\0" \
	"fdtoverlay_addr_r=0xc4100000\0" \
	"scriptaddr=0xc4100000\0" \
	"pxefile_addr_r=0xc4200000\0" \
	"splashimage=0xc4300000\0"  \
	"ramdisk_addr_r=0xc4400000\0" \
	"altbootcmd=run bootcmd\0" \
	"env_check=if env info -p -d -q; then env save; fi\0" \
	STM32MP_BOOTCMD \
	PARTS_DEFAULT \
	BOOTENV \
	"boot_net_usb_start=true\0"


#endif /* __CONFIG_H */
