/*
 * Copyright 2019 Digi International Inc
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mn_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <spl.h>
#include <asm/mach-imx/dma.h>
#include <power/pmic.h>
#include <power/bd71837.h>
#include <linux/ctype.h>

#include "ccimx8m.h"
#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/tamper.h"

DECLARE_GLOBAL_DATA_PTR;

typedef struct mac_base { uint8_t mbase[3]; } mac_base_t;

mac_base_t mac_pools[] = {
	[1] = {{0x00, 0x04, 0xf3}},
	[2] = {{0x00, 0x40, 0x9d}},
};

struct digi_hwid my_hwid;

#if !defined(CONFIG_SPL_BUILD)
int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 0;	/* index of USDHC2 (SD card) */
	case MMC3_BOOT:
		return 1;	/* index of USDHC3 (eMMC) */
	default:
		/* return default value otherwise */
		return CONFIG_SYS_MMC_ENV_DEV;
	}
}

int board_mmc_get_env_dev(int devno)
{
	return mmc_get_bootdevindex();
}

uint mmc_get_env_part(struct mmc *mmc)
{
	switch(get_boot_device()) {
	case SD1_BOOT ... SD3_BOOT:
		return 0;	/* When booting from an SD card the
				 * environment will be saved to the unique
				 * hardware partition: 0 */
	case MMC3_BOOT:
		return 2;	/* When booting from USDHC3 (eMMC) the
				 * environment will be saved to boot
				 * partition 2 to protect it from
				 * accidental overwrite during U-Boot update */
	default:
		return CONFIG_SYS_MMC_ENV_PART;
	}
}

int board_has_emmc(void)
{
	return 1;
}

static int use_mac_from_fuses(struct digi_hwid *hwid)
{
	/*
	 * Setting the mac pool to 0 means that the mac addresses will not be
	 * setup with the information encoded in the efuses.
	 * This is a back-door to allow manufacturing units with uboots that
	 * does not support some specific pool.
	 */
	return hwid->mac_pool != 0;
}

int board_has_wireless(void)
{
	if (my_hwid.ram)
		return my_hwid.wifi;

	return 1; /* assume it has, by default */
}

int board_has_bluetooth(void)
{
	if (my_hwid.ram)
		return my_hwid.bt;

	return 1; /* assume it has, by default */
}

void print_som_info(void)
{
	if (my_hwid.variant)
		printf("%s SOM variant 0x%02X: ", CONFIG_SOM_DESCRIPTION,
		       my_hwid.variant);

	if (my_hwid.ram) {
		print_size(gd->ram_size, " LPDDR4");
		if (my_hwid.wifi)
			printf(", Wi-Fi");
		if (my_hwid.bt)
			printf(", Bluetooth");
		if (my_hwid.mca)
			printf(", MCA");
		if (my_hwid.crypto)
			printf(", Crypto-auth");
		printf("\n");
	}
}

int ccimx8_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	mca_init();
	mca_somver_update();

#ifdef CONFIG_MCA_TAMPER
	mca_tamper_check_events();
#endif
	return 0;
}

void generate_partition_table(void)
{
	struct mmc *mmc = find_mmc_device(0);
	unsigned int capacity_gb = 0;
	const char *linux_partition_table;
	const char *android_partition_table;

	/* Retrieve eMMC size in GiB */
	if (mmc)
		capacity_gb = mmc->capacity / SZ_1G;

	/* eMMC capacity is not exact, so asume 16GB if larger than 15GB */
	if (capacity_gb >= 15) {
		linux_partition_table = LINUX_16GB_PARTITION_TABLE;
		android_partition_table = ANDROID_16GB_PARTITION_TABLE;
	} else if (capacity_gb >= 7) {
		linux_partition_table = LINUX_8GB_PARTITION_TABLE;
		android_partition_table = ANDROID_8GB_PARTITION_TABLE;
	} else {
		linux_partition_table = LINUX_4GB_PARTITION_TABLE;
		android_partition_table = ANDROID_4GB_PARTITION_TABLE;
	}

	if (!env_get("parts_linux"))
		env_set("parts_linux", linux_partition_table);

	if (!env_get("parts_android"))
		env_set("parts_android", android_partition_table);
}

static int set_mac_from_pool(uint32_t pool, uint8_t *mac)
{
	if (pool > ARRAY_SIZE(mac_pools) || pool < 1) {
		printf("ERROR unsupported MAC address pool %u\n", pool);
		return -EINVAL;
	}

	memcpy(mac, mac_pools[pool].mbase, sizeof(mac_base_t));

	return 0;
}

static int set_lower_mac(uint32_t val, uint8_t *mac)
{
	mac[3] = (uint8_t)(val >> 16);
	mac[4] = (uint8_t)(val >> 8);
	mac[5] = (uint8_t)(val);

	return 0;
}

static int env_set_macaddr_forced(const char *var, const uchar *enetaddr)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";

	sprintf(cmd, "setenv -f %s %pM", var, enetaddr);

	return run_command(cmd, 0);
}

static void get_macs_from_fuses(void)
{
	uint8_t macaddr[6];
	char *macvars[] = {"ethaddr", "eth1addr", "wlanaddr", "btaddr"};
	int ret, n_macs, i;

	if (!my_hwid.ram || !use_mac_from_fuses(&my_hwid))
		return;

	ret = set_mac_from_pool(my_hwid.mac_pool, macaddr);
	if (ret) {
		printf("ERROR: MAC addresses will not be set from fuses (%d)\n",
		       ret);
		return;
	}

	n_macs = board_has_wireless() ? 4 : 2;

	/* Protect from overflow */
	if (my_hwid.mac_base + n_macs > 0xffffff) {
		printf("ERROR: not enough remaining MACs on this MAC pool\n");
		return;
	}

	for (i = 0; i < n_macs; i++) {
		set_lower_mac(my_hwid.mac_base + i, macaddr);
		ret = env_set_macaddr_forced(macvars[i], macaddr);
		if (ret)
			printf("ERROR setting %s from fuses (%d)\n", macvars[i],
			       ret);
	}
}

void som_default_environment(void)
{
#ifdef CONFIG_CMD_MMC
	char cmd[80];
#endif
	char var[20];
	char hex_val[9]; // 8 hex chars + null byte
	int i;

	/* Set soc_type variable (lowercase) */
	snprintf(var, sizeof(var), "imx%s", get_imx_type(get_cpu_type()));
	for (i = 0; i < strlen(var) && var[i] != ' '; i++)
		var[i] = tolower(var[i]);
	/* Terminate string on first white space (if any) */
	var[i] = 0;
	env_set("soc_type", var);

#ifdef CONFIG_CMD_MMC
	/* Set $mmcbootdev to MMC boot device index */
	sprintf(cmd, "setenv -f mmcbootdev %x", mmc_get_bootdevindex());
	run_command(cmd, 0);
#endif
	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/* Set $hwid_n variables */
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		snprintf(var, sizeof(var), "hwid_%d", i);
		snprintf(hex_val, sizeof(hex_val), "%08x",
			 ((u32 *)&my_hwid)[i]);
		env_set(var, hex_val);
	}

	/* Set module_ram variable */
	if (my_hwid.ram) {
		int ram = hwid_get_ramsize();

		if (ram >= 1024) {
			ram /= 1024;
			snprintf(var, sizeof(var), "%dGB", ram);
		} else {
			snprintf(var, sizeof(var), "%dMB", ram);
		}
		env_set("module_ram", var);
	}

	/*
	 * If there are no defined partition tables generate them dynamically
	 * basing on the available eMMC size.
	 */
	generate_partition_table();

	/* Get MAC address from fuses unless indicated otherwise */
	if (env_get_yesno("use_fused_macs"))
		get_macs_from_fuses();

	/* Verify MAC addresses */
	verify_mac_address("ethaddr", DEFAULT_MAC_ETHADDR);
	verify_mac_address("eth1addr", DEFAULT_MAC_ETHADDR1);

	if (board_has_wireless())
		verify_mac_address("wlanaddr", DEFAULT_MAC_WLANADDR);

	if (board_has_bluetooth())
		verify_mac_address("btaddr", DEFAULT_MAC_BTADDR);
}

void board_updated_hwid(void)
{
	/* Update HWID-related variables in environment */
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return;
	}

	mca_somver_update();
	som_default_environment();
}

void fdt_fixup_ccimx8m(void *fdt)
{
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
}
#endif /* !CONFIG_SPL_BUILD */

int dram_init(void)
{
	/* rom_pointer[1] contains the size of TEE occupies */
	if (rom_pointer[1])
		gd->ram_size = PHYS_SDRAM_SIZE - rom_pointer[1];
	else
		gd->ram_size = PHYS_SDRAM_SIZE;

	return 0;
}
