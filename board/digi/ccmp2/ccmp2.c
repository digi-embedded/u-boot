// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2024, Digi International Inc - All Rights Reserved
 */
#include <command.h>
#include <common.h>
#include <display_options.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <mmc.h>
#include <linux/ctype.h>
#include <linux/delay.h>
#include <linux/sizes.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/system.h>

#include "../common/helper.h"
#include "../common/hwid.h"
#include "ccmp2.h"

static struct digi_hwid my_hwid;

/*
 * Get a global data pointer
 */
DECLARE_GLOBAL_DATA_PTR;

enum env_location env_get_location(enum env_operation op, int prio)
{
	u32 bootmode = get_bootmode();

	if (prio)
		return ENVL_UNKNOWN;

	switch (bootmode & TAMP_BOOT_DEVICE_MASK) {
	case BOOT_FLASH_SD:
	case BOOT_FLASH_EMMC:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
			return ENVL_MMC;
		else
			return ENVL_NOWHERE;

	case BOOT_FLASH_NAND:
	case BOOT_FLASH_SPINAND:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_UBI))
			return ENVL_UBI;
		else
			return ENVL_NOWHERE;

	case BOOT_FLASH_NOR:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_SPI_FLASH))
			return ENVL_SPI_FLASH;
		else
			return ENVL_NOWHERE;

	case BOOT_FLASH_HYPERFLASH:
		if (CONFIG_IS_ENABLED(ENV_IS_IN_FLASH))
			return ENVL_FLASH;
		else
			return ENVL_NOWHERE;

	default:
		return ENVL_NOWHERE;
	}
}

bool board_has_wireless(void)
{
	return my_hwid.wifi;
}

bool board_has_bluetooth(void)
{
	return my_hwid.bt;
}

int ccmp2_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	return 0;
}

void fdt_fixup_ccmp2(void *fdt)
{
	fdt_fixup_hwid(fdt, &my_hwid);

	if (board_has_wireless()) {
		/* Wireless MACs */
		fdt_fixup_mac(fdt, "wlanaddr", "/wireless", "mac-address");
		fdt_fixup_mac(fdt, "wlan1addr", "/wireless", "mac-address1");
		fdt_fixup_mac(fdt, "wlan2addr", "/wireless", "mac-address2");
		fdt_fixup_mac(fdt, "wlan3addr", "/wireless", "mac-address3");

		/* Regulatory domain */
		fdt_fixup_regulatory(fdt);
	}

	if (board_has_bluetooth())
		fdt_fixup_mac(fdt, "btaddr", "/bluetooth", "mac-address");

	fdt_fixup_uboot_info(fdt);

	/* Add DT entry to detect environment encryption in Linux */
#ifdef CONFIG_ENV_AES_CCMP1
	do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted", NULL, 0, 1);
#endif
}

bool board_has_emmc(void)
{
	return 1;
}

int mmc_get_bootdevindex(void)
{
	u32 bootmode = get_bootmode();
	
	switch (bootmode & TAMP_BOOT_DEVICE_MASK) {
	case BOOT_FLASH_EMMC:
	case BOOT_FLASH_EMMC_1:
		return 1;	/* index of SD card */
	case BOOT_FLASH_EMMC_2:
		return 2;	/* index of eMMC */
	case BOOT_FLASH_EMMC_3:
		return 2;	/* index of SDMMC3 */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

void som_default_environment(void)
{
	char var[10];
	char hex_val[9]; // 8 hex chars + null byte
	int i;

	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/* Set $hwid_n variables */
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		snprintf(var, sizeof(var), "hwid_%d", i);
		snprintf(hex_val, sizeof(hex_val), "%08x", ((u32 *) &my_hwid)[i]);
		env_set(var, hex_val);
	}

	/* Set module_ram variable */
	if (my_hwid.ram) {
		u32 ram = hwid_get_ramsize(&my_hwid);

		if (ram >= SZ_1G) {
			ram /= SZ_1G;
			snprintf(var, sizeof(var), "%uGB", ram);
		} else {
			ram /= SZ_1M;
			snprintf(var, sizeof(var), "%uMB", ram);
		}
		env_set("module_ram", var);
	}

	/* Get MAC address from fuses unless indicated otherwise */
	if (env_get_yesno("use_fused_macs"))
		hwid_get_macs(my_hwid.mac_pool, my_hwid.mac_base);

	/* Verify MAC addresses */
	verify_mac_address("ethaddr", DEFAULT_MAC_ETHADDR);
	verify_mac_address("eth1addr", DEFAULT_MAC_ETHADDR1);

	if (board_has_wireless())
		verify_mac_address("wlanaddr", DEFAULT_MAC_WLANADDR);

	if (board_has_bluetooth())
		verify_mac_address("btaddr", DEFAULT_MAC_BTADDR);

	/* Get serial number from fuses */
	hwid_get_serial_number(my_hwid.year, my_hwid.week, my_hwid.sn);
}

void board_update_hwid(bool is_fuse)
{
	/* Update HWID-related variables in environment */
	int ret = is_fuse ? board_sense_hwid(&my_hwid) : board_read_hwid(&my_hwid);

	if (ret)
		printf("Cannot read HWID\n");

	som_default_environment();
}

void print_som_info(void)
{
	if (my_hwid.variant)
		printf("%s SOM variant 0x%02X: ", CONFIG_SOM_DESCRIPTION,
		       my_hwid.variant);
	else
		return;

	if (my_hwid.ram) {
		print_size(gd->ram_size, " DDR4");
		if (my_hwid.wifi)
			printf(", Wi-Fi");
		if (my_hwid.bt)
			printf(", Bluetooth");
		if (my_hwid.mca)
			printf(", MCA");
		if (my_hwid.crypto)
			printf(", Crypto-auth");
	}
	printf("\n");
}

void print_bootinfo(void)
{
	u32 bootmode = get_bootmode();

	puts("Boot:  ");
	switch (bootmode & TAMP_BOOT_DEVICE_MASK) {
		case BOOT_FLASH_SD:
		case BOOT_FLASH_SD_1:
		case BOOT_FLASH_SD_2:
		case BOOT_FLASH_SD_3:
			puts("SD\n");
			break;
		case BOOT_FLASH_EMMC:
		case BOOT_FLASH_EMMC_1:
		case BOOT_FLASH_EMMC_2:
		case BOOT_FLASH_EMMC_3:
			puts("MMC\n");
			break;
		case BOOT_FLASH_NAND:
		case BOOT_FLASH_NAND_FMC:
			puts("NAND\n");
			break;
		case BOOT_FLASH_NOR:
		case BOOT_FLASH_NOR_QSPI:
			puts("NOR\n");
			break;
		case BOOT_SERIAL_UART:
		case BOOT_SERIAL_UART_1:
		case BOOT_SERIAL_UART_2:
		case BOOT_SERIAL_UART_3:
		case BOOT_SERIAL_UART_4:
		case BOOT_SERIAL_UART_5:
		case BOOT_SERIAL_UART_6:
		case BOOT_SERIAL_UART_7:
		case BOOT_SERIAL_UART_8:
			puts("SERIAL\n");
			break;
		case BOOT_SERIAL_USB:
		case BOOT_SERIAL_USB_OTG:
			puts("USB\n");
			break;
		case BOOT_FLASH_SPINAND:
		case BOOT_FLASH_SPINAND_1:
			puts("SPI-NAND\n");
			break;
		default:
			printf("Unknown device %u\n", bootmode);
			break;
	}
}

bool is_usb_boot(void)
{
	u32 bootmode = get_bootmode();

	switch (bootmode & TAMP_BOOT_DEVICE_MASK) {
		case BOOT_SERIAL_USB:
		case BOOT_SERIAL_USB_OTG:
			return true;
		default:
			return false;
	}
}
