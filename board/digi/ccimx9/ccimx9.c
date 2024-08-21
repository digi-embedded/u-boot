/*
 * Copyright 2022-2024 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <asm/arch/imx-regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <asm/mach-imx/boot_mode.h>
#include <command.h>
#include <common.h>
#include <display_options.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <mmc.h>

#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/mca.h"
#include "../common/trustfence.h"

static struct digi_hwid my_hwid;
static u32 soc_rev;

DECLARE_GLOBAL_DATA_PTR;

enum env_location env_get_location(enum env_operation op, int prio)
{
	enum env_location env_loc = ENVL_UNKNOWN;

	if (prio)
		return env_loc;

	if (CONFIG_IS_ENABLED(ENV_IS_IN_MMC))
		env_loc = ENVL_MMC;
	else if (CONFIG_IS_ENABLED(ENV_IS_NOWHERE))
		env_loc = ENVL_NOWHERE;

	return env_loc;
}

int mmc_get_bootdevindex(void)
{
	switch (get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC1_BOOT:
		return 0;	/* index of USDHC1 (eMMC) */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

#ifdef CONFIG_ENV_IS_IN_MMC
int board_mmc_get_env_dev(int devno)
{
	/*
	 * Use the environment from the compile time config
	 * regardless of the actual boot device.
	 */
	return CONFIG_SYS_MMC_ENV_DEV;
}
#endif

#ifdef CONFIG_FSL_ESDHC_IMX
bool board_has_emmc(void)
{
	return 1;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

void calculate_uboot_update_settings(struct blk_desc *mmc_dev,
				     struct disk_partition *info)
{
	struct mmc *mmc = find_mmc_device(EMMC_BOOT_DEV);
	int part = env_get_ulong("mmcbootpart", 10, EMMC_BOOT_PART);

	/*
	 * Use a different offset depending on the target device and partition:
	 * - For eMMC BOOT1 and BOOT2
	 *      Offset = 0
	 * - For eMMC User Data area.
	 *      Offset = EMMC_BOOT_PART_OFFSET
	 */
	if (is_imx9() && (part == 1 || part == 2)) {
		/* eMMC BOOT1 or BOOT2 partitions */
		info->start = 0;
	} else {
		info->start = EMMC_BOOT_PART_OFFSET / mmc_dev->blksz;
	}

	/* Boot partition size - Start of boot image */
	info->size = (mmc->capacity_boot / mmc_dev->blksz) - info->start;
}

bool board_has_eth1(void)
{
	return true;
}

static bool board_has_mca(void)
{
	return my_hwid.mca;
}

bool board_has_wireless(void)
{
	return my_hwid.wifi;
}

bool board_has_bluetooth(void)
{
	return my_hwid.bt;
}

bool board_has_npu(void)
{
	return is_imx9321() || is_imx9322() || is_imx9351() || is_imx9352();
}

static const char *get_cpu_type_str(void)
{
	u32 cpu_type = get_cpu_type();

	switch (cpu_type) {
	case MXC_CPU_IMX93:
		return "imx9352";
	case MXC_CPU_IMX9351:
		return "imx9351";
	case MXC_CPU_IMX9332:
		return "imx9332";
	case MXC_CPU_IMX9331:
		return "imx9331";
	case MXC_CPU_IMX9322:
		return "imx9322";
	case MXC_CPU_IMX9321:
		return "imx9321";
	case MXC_CPU_IMX9312:
		return "imx9312";
	case MXC_CPU_IMX9311:
		return "imx9311";
	default:
		return "";
	}
}

int ccimx9_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

#ifdef CONFIG_MCA
	if (board_has_mca()) {
		mca_init();
		mca_somver_update(&my_hwid);
#ifdef CONFIG_MCA_TAMPER
		mca_tamper_check_events();
#endif
	}
#endif

	soc_rev = soc_rev();

	return 0;
}

void som_default_environment(void)
{
#ifdef CONFIG_CMD_MMC
	char cmd[80];
#endif
	char var[200];
	char hex_val[9]; // 8 hex chars + null byte
	int i;

#ifdef CONFIG_CMD_MMC
	/* Set $mmcbootdev to MMC boot device index */
	sprintf(cmd, "setenv -f mmcbootdev %x", mmc_get_bootdevindex());
	run_command(cmd, 0);
#endif

	/* Set module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/* Set soc_rev variable */
	sprintf(var, "0x%02x", soc_rev);
	env_set("soc_rev", var);

	/* Set hwid_n variables */
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		snprintf(var, sizeof(var), "hwid_%d", i);
		snprintf(hex_val, sizeof(hex_val), "%08x",
			 ((u32 *) & my_hwid)[i]);
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

	/* Set 'som_overlays' variable */
	var[0] = 0;
	if (board_has_wireless())
		strlcat(var, "_ov_som_wifi_ccimx93.dtbo,", sizeof(var));
	if (board_has_bluetooth())
		strlcat(var, "_ov_som_bt_ccimx93.dtbo,", sizeof(var));
	if (board_has_npu())
		strlcat(var, "_ov_som_npu_ccimx93.dtbo,", sizeof(var));
	/* Remove the trailing comma */
	if (var[0])
		var[strlen(var) - 1] = 0;
	env_set("som_overlays", var);

	env_set("cpu_type", get_cpu_type_str());
}

void board_update_hwid(bool is_fuse)
{
	/* Update HWID-related variables in MCA and environment */
	int ret =
	    is_fuse ? board_sense_hwid(&my_hwid) : board_read_hwid(&my_hwid);

	if (ret)
		printf("Cannot read HWID\n");

#ifdef CONFIG_MCA
	if (board_has_mca())
		mca_somver_update(&my_hwid);
#endif

	som_default_environment();
}

int ccimx9_late_init(void)
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

void fdt_fixup_ccimx9(void *fdt)
{
	fdt_fixup_hwid(fdt, &my_hwid);

	if (soc_rev) {
		char hex_rev[5]; // 4 hex chars + null byte
		snprintf(hex_rev, sizeof(hex_rev), "0x%02x", soc_rev);
		do_fixup_by_path(fdt, "/soc", "revision", hex_rev,
				 sizeof(hex_rev), 1);
	}

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

	print_size(gd->ram_size, " LPDDR4");
	if (my_hwid.wifi)
		printf(", Wi-Fi");
	if (my_hwid.bt)
		printf(", Bluetooth");
	if (board_has_mca())
		printf(", MCA");
	if (my_hwid.crypto)
		printf(", Crypto-auth");
	printf("\n");
}

int print_bootinfo(void)
{
	enum boot_device bt_dev = get_boot_device();

	puts("Boot:  ");
	switch (bt_dev) {
	case SD1_BOOT:
		puts("SD0\n");
		break;
	case SD2_BOOT:
		puts("SD1\n");
		break;
	case SD3_BOOT:
		puts("SD2\n");
		break;
	case MMC1_BOOT:
		puts("MMC0\n");
		break;
	case MMC2_BOOT:
		puts("MMC1\n");
		break;
	case MMC3_BOOT:
		puts("MMC2\n");
		break;
	case NAND_BOOT:
		puts("NAND\n");
		break;
	case USB_BOOT:
		puts("USB\n");
		break;
	default:
		printf("Unknown device %u\n", bt_dev);
		break;
	}

	return 0;
}
