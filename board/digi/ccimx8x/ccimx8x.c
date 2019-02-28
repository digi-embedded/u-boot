/*
 * Copyright (C) 2018 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_esdhc.h>
#include <i2c.h>
#include <otf_update.h>

#include <asm/mach-imx/sci/sci.h>
#include <asm/mach-imx/boot_mode.h>
#include "../../freescale/common/tcpc.h"

#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/tamper.h"
#include "ccimx8x.h"

DECLARE_GLOBAL_DATA_PTR;

typedef struct mac_base { uint8_t mbase[3]; } mac_base_t;

mac_base_t mac_pools[] = {
	[1] = {{0x00, 0x04, 0xf3}},
	[2] = {{0x00, 0x40, 0x9d}},
};

struct digi_hwid my_hwid;

#define MCA_CC8X_DEVICE_ID_VAL		0x4A

#ifdef CONFIG_FSL_ESDHC

static struct ccimx8_variant ccimx8x_variants[] = {
/* 0x00 */ { IMX8_NONE,	0, 0, "Unknown"},
/* 0x01 - 55001984-01 */
	{
		IMX8QXP,
		MEM_1GB,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Automotive QuadXPlus 1.2GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x02 - 55001984-02 */
	{
		IMX8QXP,
		MEM_2GB,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Automotive QuadXPlus 1.2GHz, 16GB eMMC, 2GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x03 - 55001984-03 */
	{
		IMX8QXP,
		MEM_2GB,
		0,
		"Automotive QuadXPlus 1.2GHz, 8GB eMMC, 2GB LPDDR4, -40/+85C",
	},
/* 0x04 - 55001984-04 */
	{
		IMX8DXP,
		MEM_1GB,
		CCIMX8_HAS_WIRELESS | CCIMX8_HAS_BLUETOOTH,
		"Automotive DualXPlus 1.2GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C, Wireless, Bluetooth",
	},
/* 0x05 - 55001984-05 */
	{
		IMX8DXP,
		MEM_1GB,
		0,
		"Automotive DualXPlus 1.2GHz, 8GB eMMC, 1GB LPDDR4, -40/+85C",
	},
};

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD2_BOOT:
		return 1;	/* index of USDHC2 (SD card) */
	case MMC1_BOOT:
		return 0;	/* index of USDHC1 (eMMC) */
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
	case SD2_BOOT:
		return 0;	/* When booting from an SD card the
				 * environment will be saved to the unique
				 * hardware partition: 0 */
	case MMC1_BOOT:
		return 2;	/* When booting from USDHC1 (eMMC) the
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

#endif /* CONFIG_FSL_ESDHC */

static void mca_init(void)
{
	unsigned char devid = 0;
	unsigned char hwver;
	unsigned char fwver[2];
	unsigned char somver;
	int ret, fwver_ret, somver_ret;
	struct digi_hwid hwid;

#ifdef CONFIG_DM_I2C
	struct udevice *bus, *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_MCA_I2C_BUS, &bus);
	if (ret) {
		printf("ERROR: getting %d bus for MCA\n", CONFIG_MCA_I2C_BUS);
		return;
	}

	ret = dm_i2c_probe(bus, CONFIG_MCA_I2C_ADDR, 0, &dev);
	if (ret) {
		printf("ERROR: can't find MCA at address %x\n",
		       CONFIG_MCA_I2C_ADDR);
		return;
	}

	ret = i2c_set_chip_offset_len(dev, 2);
	if (ret)
		printf("ERROR: setting address len to 2 for MCA\n");
#endif
	ret = mca_read_reg(MCA_DEVICE_ID, &devid);
	if (devid != MCA_CC8X_DEVICE_ID_VAL) {
		printf("MCA: invalid MCA DEVICE ID (0x%02x)\n", devid);
		return;
	}

	ret = mca_read_reg(MCA_HW_VER, &hwver);
	fwver_ret = mca_bulk_read(MCA_FW_VER_L, fwver, 2);

	printf("MCA:   HW_VER=");
	if (ret)
		printf("??");
	else
		printf("%d", hwver);

	printf("  FW_VER=");
	if (fwver_ret)
		printf("??");
	else
		printf("%d.%02d %s", fwver[1] & 0x7f, fwver[0],
		       fwver[1] & 0x80 ? "(alpha)" : "");

	printf("\n");

	/*
	 * Read the som version stored in mca.
	 * If it doesn't match with real som version read from hwid.hv:
	 *    - update it into the mca.
	 *    - force the new value to be saved in mca nvram.
	 * The purpose of this functionality is that mca starts using the
	 * correct som version since boot.
	 */
	somver_ret = mca_read_reg(MCA_HWVER_SOM, &somver);
	if (somver_ret)
		printf("Cannot read MCA_HWVER_SOM\n");
	else {
		if (board_read_hwid(&hwid))
			printf("Cannot read HWID\n");
		else {
			if (hwid.hv != somver) {
				somver_ret = mca_write_reg(MCA_HWVER_SOM, hwid.hv);
				if (somver_ret)
					printf("Cannot write MCA_HWVER_SOM\n");
				else
					mca_save_cfg();
			}
		}
	}
}

static int is_valid_hwid(struct digi_hwid *hwid)
{
	if (hwid->variant < ARRAY_SIZE(ccimx8x_variants))
		if (ccimx8x_variants[hwid->variant].cpu != IMX8_NONE)
			return 1;

	return 0;
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
	if (is_valid_hwid(&my_hwid))
		return (ccimx8x_variants[my_hwid.variant].capabilities &
				CCIMX8_HAS_WIRELESS);
	else
		return 1; /* assume it has if invalid HWID */
}

int board_has_bluetooth(void)
{
	if (is_valid_hwid(&my_hwid))
		return (ccimx8x_variants[my_hwid.variant].capabilities &
				CCIMX8_HAS_BLUETOOTH);
	else
		return 1; /* assume it has if invalid HWID */
}

void print_ccimx8x_info(void)
{
	if (is_valid_hwid(&my_hwid))
		printf("%s SOM variant 0x%02X: %s\n", CONFIG_SOM_DESCRIPTION,
			my_hwid.variant,
			ccimx8x_variants[my_hwid.variant].id_string);
}

int ccimx8_init(void)
{
	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	mca_init();

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

void som_default_environment(void)
{
#ifdef CONFIG_CMD_MMC
	char cmd[80];
#endif
	char var[10];

#ifdef CONFIG_CMD_MMC
	/* Set $mmcbootdev to MMC boot device index */
	sprintf(cmd, "setenv -f mmcbootdev %x", mmc_get_bootdevindex());
	run_command(cmd, 0);
#endif
	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/*
	 * If there are no defined partition tables generate them dynamically
	 * basing on the available eMMC size.
	 */
	generate_partition_table();
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

	if (!is_valid_hwid(&my_hwid) || !use_mac_from_fuses(&my_hwid))
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

int ccimx8x_late_init(void)
{
	if (env_get_yesno("use_fused_macs"))
		get_macs_from_fuses();

	/* Verify MAC addresses */
	verify_mac_address("ethaddr", DEFAULT_MAC_ETHADDR);
	verify_mac_address("eth1addr", DEFAULT_MAC_ETHADDR1);

	if (board_has_wireless())
		verify_mac_address("wlanaddr", DEFAULT_MAC_WLANADDR);

	if (board_has_bluetooth())
		verify_mac_address("btaddr", DEFAULT_MAC_BTADDR);

	return 0;
}

/*
 * Board specific reset that is system reset.
 */
void reset_cpu(ulong addr)
{
	puts("SCI reboot request");
	sc_pm_reboot(SC_IPC_CH, SC_PM_RESET_TYPE_COLD);
	while (1)
		putc('.');
}

/*
 * Use the reset_misc hook to customize the reset implementation. This function
 * is invoked before the standard reset_cpu().
 * On the CC8X we want to perfrom the reset through the MCA which may, depending
 * on the configuration, assert the POR_B line or perform a power cycle of the
 * system.
 */
void reset_misc(void)
{
#if 0
	/*
	 * If a bmode_reset was flagged, do not reset through the MCA, which
	 * would otherwise power-cycle the CPU.
	 */
	if (bmode_reset)
		return;
#endif
	mca_reset();

	mdelay(1);

#ifdef CONFIG_PSCI_RESET
	psci_system_reset();
	mdelay(1);
#endif
}

void fdt_fixup_ccimx8x(void *fdt)
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

void detail_board_ddr_info(void)
{
	puts("\nDDR    ");
}
