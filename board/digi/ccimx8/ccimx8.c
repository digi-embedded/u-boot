/*
 * Copyright (C) 2019 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_esdhc_imx.h>
#include <i2c.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
#include <asm/arch/sys_proto.h>

#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/mca.h"
#include "../common/tamper.h"
#ifdef CONFIG_HAS_TRUSTFENCE
#include "../common/trustfence.h"
#endif
#include "ccimx8.h"

#ifdef CONFIG_CC8X
extern const char *get_imx8_rev(u32 rev);
extern const char *get_imx8_type(u32 imxtype);
extern struct ccimx8_variant ccimx8x_variants[];
#endif
static struct digi_hwid my_hwid;

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_FSL_ESDHC_IMX
int board_mmc_get_env_dev(int devno)
{
	return mmc_get_bootdevindex();
}

bool board_has_emmc(void)
{
	return 1;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

bool board_has_wireless(void)
{
	if (my_hwid.ram)
		return my_hwid.wifi;

#ifdef CONFIG_CC8X
	if (hwid_in_db(my_hwid.variant))
		return !!(ccimx8x_variants[my_hwid.variant].capabilities &
				CCIMX8_HAS_WIRELESS);
#endif

	return true; /* assume it has, by default */
}

bool board_has_bluetooth(void)
{
	if (my_hwid.ram)
		return my_hwid.bt;

#ifdef CONFIG_CC8X
	if (hwid_in_db(my_hwid.variant))
		return !!(ccimx8x_variants[my_hwid.variant].capabilities &
			  CCIMX8_HAS_BLUETOOTH);
#endif

	return true; /* assume it has, by default */
}

bool board_has_eth1(void)
{
#ifdef CONFIG_CC8M
	return false;
#endif
	return true;
}

int ccimx8_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	mca_init();
	mca_somver_update(&my_hwid);

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
	const char *linux_dualboot_partition_table;

	/* Retrieve eMMC size in GiB */
	if (mmc)
		capacity_gb = mmc->capacity / SZ_1G;

	/* eMMC capacity is not exact, so asume 32GB if larger than 28GB */
	if (capacity_gb >= 28) {
		linux_partition_table = LINUX_32GB_PARTITION_TABLE;
		android_partition_table = ANDROID_32GB_PARTITION_TABLE;
		linux_dualboot_partition_table = LINUX_DUALBOOT_32GB_PARTITION_TABLE;
	} else if (capacity_gb >= 14) {
		linux_partition_table = LINUX_16GB_PARTITION_TABLE;
		android_partition_table = ANDROID_16GB_PARTITION_TABLE;
		linux_dualboot_partition_table = LINUX_DUALBOOT_16GB_PARTITION_TABLE;
	} else if (capacity_gb >= 7) {
		linux_partition_table = LINUX_8GB_PARTITION_TABLE;
		android_partition_table = ANDROID_8GB_PARTITION_TABLE;
		linux_dualboot_partition_table = LINUX_DUALBOOT_8GB_PARTITION_TABLE;
	} else {
		linux_partition_table = LINUX_4GB_PARTITION_TABLE;
		android_partition_table = ANDROID_4GB_PARTITION_TABLE;
		linux_dualboot_partition_table = LINUX_DUALBOOT_4GB_PARTITION_TABLE;
	}

	if (!env_get("parts_linux"))
		env_set("parts_linux", linux_partition_table);

	if (!env_get("parts_android"))
		env_set("parts_android", android_partition_table);

	if (!env_get("parts_linux_dualboot"))
		env_set("parts_linux_dualboot", linux_dualboot_partition_table);
}

void som_default_environment(void)
{
#ifdef CONFIG_CMD_MMC
	char cmd[80];
#endif
	char var[200], somtype;
	char hex_val[9]; // 8 hex chars + null byte
	int i;

	/* Set soc_type variable (lowercase) */
#ifdef CONFIG_CC8X
	snprintf(var, sizeof(var), "imx8%s", get_imx8_type(get_cpu_type()));
	for (i = 0; i < strlen(var); i++)
		var[i] = tolower(var[i]);
	somtype = 'x';
#else
	switch (get_cpu_type()) {
		case MXC_CPU_IMX8MN:
		case MXC_CPU_IMX8MND:
		case MXC_CPU_IMX8MNS:
		case MXC_CPU_IMX8MNL:
		case MXC_CPU_IMX8MNDL:
		case MXC_CPU_IMX8MNSL:
			snprintf(var, sizeof(var), "imx8mn");
			break;
		case MXC_CPU_IMX8MM:
		case MXC_CPU_IMX8MML:
		case MXC_CPU_IMX8MMD:
		case MXC_CPU_IMX8MMDL:
		case MXC_CPU_IMX8MMS:
		case MXC_CPU_IMX8MMSL:
			snprintf(var, sizeof(var), "imx8mm");
			break;
		default:
			snprintf(var, sizeof(var), "imx%s", get_imx_type(get_cpu_type()));
	}
	somtype = 'm';
	for (i = 0; i < strlen(var) && var[i] != ' '; i++)
		var[i] = tolower(var[i]);
	/* Terminate string on first white space (if any) */
	var[i] = 0;
#endif
	env_set("soc_type", var);

#ifdef CONFIG_CC8X
	snprintf(var, sizeof(var), "%s0", get_imx8_rev(get_cpu_rev() & 0xFFF));
	env_set("soc_rev", var);
#endif

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
		u64 ram = hwid_get_ramsize(&my_hwid);

		if (ram >= SZ_1G) {
			ram /= SZ_1G;
			snprintf(var, sizeof(var), "%lluGB", ram);
		} else {
			ram /= SZ_1M;
			snprintf(var, sizeof(var), "%lluMB", ram);
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

	/* Set 'som_overlays' variable (used to boot android) */
	var[0] = 0;
	switch (get_cpu_type()) {
		case MXC_CPU_IMX8QXP:
			snprintf(var, sizeof(var),
				 "_ov_som_quad_ccimx8%c.dtbo,", somtype);
			break;
	}

	if (board_has_wireless())
		snprintf(var + strlen(var), sizeof(var) - strlen(var),
			 "_ov_som_wifi_ccimx8%c.dtbo,", somtype);

	if (board_has_bluetooth())
		snprintf(var + strlen(var), sizeof(var) - strlen(var),
			 "_ov_som_bt_ccimx8%c.dtbo,", somtype);

#ifdef CONFIG_IMX_TRUSTY_OS
	snprintf(var + strlen(var), sizeof(var) - strlen(var),
		 "_ov_som_trusty_ccimx8%c.dtbo,", somtype);
#endif

	/* Remove the trailing comma */
	var[strlen(var) - 1] = 0;
	env_set("som_overlays", var);
}

void board_update_hwid(bool is_fuse)
{
	/* Update HWID-related variables in MCA and environment */
	int ret = is_fuse ? board_sense_hwid(&my_hwid) : board_read_hwid(&my_hwid);

	if (ret)
		printf("Cannot read HWID\n");

	mca_somver_update(&my_hwid);
	som_default_environment();
}

int ccimx8_late_init(void)
{
#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
	gd->flags &= ~GD_FLG_DISABLE_CONSOLE_INPUT;
	if (!console_enable_passphrase())
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
	else
		gd->flags |= GD_FLG_DISABLE_CONSOLE_INPUT;
#endif

	return 0;
}

void fdt_fixup_ccimx8(void *fdt)
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

#ifdef CONFIG_HAS_TRUSTFENCE
	fdt_fixup_trustfence(fdt);
#endif
	fdt_fixup_uboot_info(fdt);
}

void print_som_info(void)
{
	if (my_hwid.variant)
		printf("%s SOM variant 0x%02X: ", CONFIG_SOM_DESCRIPTION,
		       my_hwid.variant);
	else
		return;

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
	}
#ifdef CONFIG_CC8X
	else if (hwid_in_db(my_hwid.variant)) {
		printf("%s", ccimx8x_variants[my_hwid.variant].id_string);
	}
#endif
	printf("\n");
}

/*
 * Use the reset_misc hook to customize the reset implementation. This function
 * is invoked before the standard reset_cpu().
 * We want to perform the reset through the MCA which may, depending
 * on the configuration, assert the POR_B line or perform a power cycle of the
 * system.
 */
void reset_misc(void)
{
	mca_reset();
	mdelay(1);

	/* fall back to regular reset if MCA reset doesn't work */
#ifdef CONFIG_PSCI_RESET
	psci_system_reset();
	mdelay(1);
#endif
}
