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

/*
 * default bootcmd for stm32mp25:
 * for serial/usb: execute the stm32prog command
 * for mmc boot (eMMC, SD card), distro boot on the same mmc device
 * for NAND or SPI-NAND boot, distro boot with UBIFS on UBI partition
 * for other boot, use the default distro order in ${boot_targets}
 */
#define STM32MP_BOOTCMD "bootcmd_stm32mp=" \
	"echo \"Boot over ${boot_device}${boot_instance}!\";" \
	"if test ${boot_device} = serial || test ${boot_device} = usb;" \
	"then stm32prog ${boot_device} ${boot_instance}; " \
	"else " \
		"run env_check;" \
		"if test ${boot_device} = mmc;" \
		"then env set boot_targets \"mmc${boot_instance}\"; fi;" \
		"if test ${boot_device} = nand ||" \
		  " test ${boot_device} = spi-nand ;" \
		"then env set boot_targets ubifs0; fi;" \
		"run distro_bootcmd;" \
	"fi;\0"

#ifndef STM32MP_BOARD_EXTRA_ENV
#define STM32MP_BOARD_EXTRA_ENV
#endif

#define STM32MP_EXTRA \
	"env_check=if env info -p -d -q; then env save; fi\0" \
	"boot_net_usb_start=true\0"



#include <config_distro_bootcmd.h>
#define CONFIG_EXTRA_ENV_SETTINGS \
	STM32MP_MEM_LAYOUT \
	STM32MP_BOOTCMD \
	BOOTENV \
	STM32MP_EXTRA \
	STM32MP_BOARD_EXTRA_ENV

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
