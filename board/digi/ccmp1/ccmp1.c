// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2022-2023, Digi International Inc - All Rights Reserved
 */
#include <common.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <nand.h>
#include <asm/arch/sys_proto.h>

#include "../common/helper.h"
#include "../common/hwid.h"
#include "ccmp1.h"

static struct digi_hwid my_hwid;

enum env_location env_get_location(enum env_operation op, int prio)
{
	if (prio)
		return ENVL_UNKNOWN;

	if (CONFIG_IS_ENABLED(ENV_IS_IN_UBI))
		return ENVL_UBI;
	else
		return ENVL_NOWHERE;
}

bool board_has_eth1(void)
{
	if (CONFIG_IS_ENABLED(TARGET_CCMP13_DVK))
		return true;
	else
		return false;
}

bool board_has_wireless(void)
{
	return my_hwid.wifi;
}

bool board_has_bluetooth(void)
{
	return my_hwid.bt;
}

int ccmp1_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	return 0;
}

int fdt_fixup_memory_gpu(void *fdt, u64 start, u64 size)
{
	return fdt_fixup_reg_banks(fdt, &start, &size, 1, "reserved-memory", "gpu");
}

int fdt_fixup_memory_optee(void *fdt, u64 start, u64 size)
{
	return fdt_fixup_reg_banks(fdt, &start, &size, 1, "reserved-memory", "optee");
}

void fdt_fixup_memory_node_ccmp1(void *fdt)
{
	u32 optee_base = 0;
	u32 optee_size = SZ_32M;
	u32 gpu_base = 0;
	u32 gpu_size = SZ_64M;
	u32 ram_size = 0;
	int ret = 0;

	if (my_hwid.ram) {
		/* Set memory node based on HWID info */
		ram_size = hwid_get_ramsize(&my_hwid);
		ret = fdt_fixup_memory(fdt, (u64)CONFIG_SYS_SDRAM_BASE, (u64)ram_size);
		if (ret < 0)
			printf("%s(): Failed to fixup memory node\n", __func__);

		/* Reserve last 32 MiB for OPTEE */
		optee_base = (CONFIG_SYS_SDRAM_BASE + ram_size) - optee_size;
		ret = fdt_fixup_memory_optee(fdt, (u64)optee_base, (u64)optee_size);
		if (ret < 0)
			printf("%s(): Failed to fixup optee node\n", __func__);

		/* Reserve previous 64 MiB for GPU */
		gpu_base = (CONFIG_SYS_SDRAM_BASE + ram_size) - (gpu_size + optee_size);
		ret = fdt_fixup_memory_gpu(fdt, (u64)gpu_base, (u64)gpu_size);
		if (ret < 0)
			printf("%s(): Failed to fixup gpu node\n", __func__);
	}
}

void fdt_fixup_ccmp1(void *fdt)
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
	do_fixup_by_path(fdt, "/", "digi,uboot-env,encrypted-optee", NULL, 0, 1);
#endif
	fdt_fixup_memory_node_ccmp1(fdt);
}

#define MTDPARTS_LEN		256
void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	struct mtd_info *nand = get_nand_dev_by_index(0);
	static char parts[3 * MTDPARTS_LEN + 1];
	static char ids[] = CONFIG_MTDIDS_DEFAULT;
	static bool mtd_initialized;

	if (mtd_initialized) {
		*mtdids = ids;
		*mtdparts = parts;
		return;
	}

	memset(parts, 0, sizeof(parts));

	if (nand->size > SZ_512M)
		strcat(parts, "mtdparts=nand0:" CONFIG_MTDPARTS_NAND0_BOOT ","
		       MTDPARTS_1024M);
	else if (nand->size > SZ_256M)
		strcat(parts, "mtdparts=nand0:" CONFIG_MTDPARTS_NAND0_BOOT ","
		       MTDPARTS_512M);
	else
		strcat(parts, "mtdparts=nand0:" CONFIG_MTDPARTS_NAND0_BOOT ","
		       MTDPARTS_256M);

	*mtdparts = parts;
	*mtdids = ids;
	mtd_initialized = true;
}

void generate_ubi_volumes_script(void)
{
	struct mtd_info *nand = get_nand_dev_by_index(0);
	char script[CONFIG_SYS_CBSIZE] = "";

	if (nand->size > SZ_512M) {
		sprintf(script, CREATE_UBIVOLS_SCRIPT,
				UBIVOLS1_DUALBOOT_1024MB,
				UBIVOLS1_1024MB,
				UBIVOLS2_DUALBOOT_1024MB,
				UBIVOLS2_1024MB);
	} else if (nand->size > SZ_256M) {
		sprintf(script, CREATE_UBIVOLS_SCRIPT,
				UBIVOLS1_DUALBOOT_512MB,
				UBIVOLS1_512MB,
				UBIVOLS2_DUALBOOT_512MB,
				UBIVOLS2_512MB);
	} else {
		sprintf(script, CREATE_UBIVOLS_SCRIPT,
				UBIVOLS1_DUALBOOT_256MB,
				UBIVOLS1_256MB,
				UBIVOLS2_DUALBOOT_256MB,
				UBIVOLS2_256MB);
	}
	env_set("ubivolscript", script);
}

void som_default_environment(void)
{
	char var[10];
	char hex_val[9]; // 8 hex chars + null byte
	int i;

	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/* UBI volumes */
	generate_ubi_volumes_script();

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

	if (board_has_eth1())
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
		print_size(gd->ram_size, " DDR3");
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
