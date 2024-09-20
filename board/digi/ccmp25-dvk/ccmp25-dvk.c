// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
/*
 * Copyright (C) 2022, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2024, Digi International Inc - All Rights Reserved
 */

#define LOG_CATEGORY LOGC_BOARD

#include <common.h>
#include <button.h>
#include <config.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_support.h>
#include <g_dnl.h>
#include <i2c.h>
#include <led.h>
#include <log.h>
#include <misc.h>
#include <mmc.h>
#include <init.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/io.h>
#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/arch/sys_proto.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <dm/ofnode.h>
#include <dm/uclass.h>
#include <dt-bindings/gpio/gpio.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>

#include "../ccmp2/ccmp2.h"
#include "../common/carrier_board.h"

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

#define SYSCFG_ETHCR_ETH_SEL_MII	0
#define SYSCFG_ETHCR_ETH_SEL_RGMII	BIT(4)
#define SYSCFG_ETHCR_ETH_SEL_RMII	BIT(6)
#define SYSCFG_ETHCR_ETH_CLK_SEL	BIT(1)
#define SYSCFG_ETHCR_ETH_REF_CLK_SEL	BIT(0)
/* CLOCK feed to PHY*/
#define ETH_CK_F_25M	25000000
#define ETH_CK_F_50M	50000000
#define ETH_CK_F_125M	125000000

/*
 * Get a global data pointer
 */
DECLARE_GLOBAL_DATA_PTR;

int checkboard(void)
{
	board_version = get_carrierboard_version();
	board_id = get_carrierboard_id();

	print_som_info();
	print_carrierboard_info();
	print_bootinfo();

	return 0;
}

#ifdef CONFIG_USB_GADGET_DOWNLOAD
#define STM32MP1_G_DNL_DFU_PRODUCT_NUM 0xdf11
#define STM32MP2_G_DNL_FASTBOOT_PRODUCT_NUM 0x0afb

int g_dnl_bind_fixup(struct usb_device_descriptor *dev, const char *name)
{
	if (IS_ENABLED(CONFIG_DFU_OVER_USB) &&
	    !strcmp(name, "usb_dnl_dfu"))
		put_unaligned(STM32MP1_G_DNL_DFU_PRODUCT_NUM, &dev->idProduct);
	else if (IS_ENABLED(CONFIG_FASTBOOT) &&
		 !strcmp(name, "usb_dnl_fastboot"))
		put_unaligned(STM32MP2_G_DNL_FASTBOOT_PRODUCT_NUM,
			      &dev->idProduct);
	else
		put_unaligned(CONFIG_USB_GADGET_PRODUCT_NUM, &dev->idProduct);

	return 0;
}
#endif /* CONFIG_USB_GADGET_DOWNLOAD */

/*
 * RV-3028-C7 application manual
 *
 * 4.6.3 UPDATE (ALL CONFIGURATION RAM -> EEPROM)
 *
 * - Before starting to change the configuration stored in the EEPROM, the auto
 *   refresh of the registers from the EEPROM has to be disabled by writing 1
 *   into the EERD control bit.
 * - Then the new configuration can be written into the configuration RAM
 *   registers, when the whole new configuration is in the registers, writing
 *   the command 00h into the register EECMD, then the second command 11h into
 *   the register EECMD will start the copy of the configuration into the EEPROM.
 * - The time of the update is tUPDATE = ~63 ms.
 * - When the transfer is finished (EEbusy = 0), the user can enable again the
 *   auto refresh of the registers by writing 0 into the EERD bit in the Control
 *   1 register.
 */
/* from drivers/rtc/rv3028.c */
#define RV3028_STATUS			0x0E
#define RV3028_CTRL1			0x0F
#define RV3028_RAM1			0x1F
#define RV3028_EEPROM_CMD		0x27
#define RV3028_BACKUP			0x37
#define RV3028_CTRL1_EERD		BIT(3)
#define RV3028_EEPROM_CMD_UPDATE	0x11
#define RV3028_EEBUSY_TIMEOUT		100000
#define RV3028_BACKUP_BSM		GENMASK(3, 2)
#define RV3028_BACKUP_BSM_LSM		0x3

static int setup_rv3028(void)
{
	struct udevice *rtc = NULL;
	u8 bsm;
	int ret;

	ret = uclass_get_device_by_name(UCLASS_RTC, "rtc@52", &rtc);
	if (ret)
		return -ENODEV;

	/* Check BSM configuration */
	bsm = dm_i2c_reg_read(rtc, RV3028_BACKUP);
	if (bsm < 0) {
		return -EIO;
	} else if (FIELD_GET(RV3028_BACKUP_BSM, bsm) == RV3028_BACKUP_BSM_LSM) {
		debug("%s: BSM already configured to LSM\n", __func__);
		return 0;
	}

	debug("%s: configure RTC's BSM to LSM\n", __func__);

	/* Disable auto refresh */
	ret =
	    dm_i2c_reg_clrset(rtc, RV3028_CTRL1, RV3028_CTRL1_EERD,
			      RV3028_CTRL1_EERD);
	if (ret < 0)
		return -EIO;

	/* Enable BSM to level switching mode (LSM) */
	ret =
	    dm_i2c_reg_clrset(rtc, RV3028_BACKUP, RV3028_BACKUP_BSM,
			      FIELD_PREP(RV3028_BACKUP_BSM,
					 RV3028_BACKUP_BSM_LSM));
	if (ret < 0)
		goto restore_eerd;

	/* Update EEPROM */
	ret = dm_i2c_reg_write(rtc, RV3028_EEPROM_CMD, 0x00);
	if (ret < 0)
		goto restore_eerd;
	ret =
	    dm_i2c_reg_write(rtc, RV3028_EEPROM_CMD, RV3028_EEPROM_CMD_UPDATE);
	if (ret < 0)
		goto restore_eerd;

	udelay(RV3028_EEBUSY_TIMEOUT);

restore_eerd:
	/* Enable auto refresh and return */
	return (dm_i2c_reg_clrset(rtc, RV3028_CTRL1, RV3028_CTRL1_EERD, 0) <
		0) ? -1 : ret;
}

/* board dependent setup after realloc */
int board_init(void)
{
	/* SOM init */
	ccmp2_init();

	if (IS_ENABLED(CONFIG_RTC_RV3028))
		setup_rv3028();

	return 0;
}

/* eth init function : weak called in eqos driver */
int board_interface_eth_init(struct udevice *dev,
			     phy_interface_t interface_type, ulong rate)
{
	struct regmap *regmap;
	uint regmap_mask, regmap_offset;
	int ret;
	u32 value;
	bool ext_phyclk;

	/* Ethernet PHY have no cristal or need to be clock by RCC */
	ext_phyclk = dev_read_bool(dev, "st,ext-phyclk");

	regmap = syscon_regmap_lookup_by_phandle(dev,"st,syscon");

	if (!IS_ERR(regmap)) {
		u32 fmp[3];

		ret = dev_read_u32_array(dev, "st,syscon", fmp, 3);
		if (ret) {
			pr_err("%s: Need to specify Offset and Mask of syscon register\n", __func__);
			return ret;
		}
		else {
			regmap_mask = fmp[2];
			regmap_offset = fmp[1];
		}
	} else
		return -ENODEV;

	switch (interface_type) {
	case PHY_INTERFACE_MODE_MII:
		value = SYSCFG_ETHCR_ETH_SEL_MII;
		debug("%s: PHY_INTERFACE_MODE_MII\n", __func__);
		break;
	case PHY_INTERFACE_MODE_RMII:
		if ((rate == ETH_CK_F_50M) && ext_phyclk)
			value = SYSCFG_ETHCR_ETH_SEL_RMII |
				SYSCFG_ETHCR_ETH_REF_CLK_SEL;
		else
			value = SYSCFG_ETHCR_ETH_SEL_RMII;
		debug("%s: PHY_INTERFACE_MODE_RMII\n", __func__);
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		if ((rate == ETH_CK_F_125M) && ext_phyclk)
			value = SYSCFG_ETHCR_ETH_SEL_RGMII |
				SYSCFG_ETHCR_ETH_CLK_SEL;
		else
			value = SYSCFG_ETHCR_ETH_SEL_RGMII;
		debug("%s: PHY_INTERFACE_MODE_RGMII\n", __func__);
		break;
	default:
		debug("%s: Do not manage %d interface\n",
		      __func__, interface_type);
		/* Do not manage others interfaces */
		return -EINVAL;
	}

	ret = regmap_update_bits(regmap, regmap_offset, regmap_mask, value);

	return ret;
}

int mmc_get_env_dev(void)
{
	const int mmc_env_dev = CONFIG_IS_ENABLED(ENV_IS_IN_MMC, (CONFIG_SYS_MMC_ENV_DEV), (-1));

	if (mmc_env_dev >= 0)
		return mmc_env_dev;

	/* use boot instance to select the correct mmc device identifier */
	return mmc_get_boot();
}

void platform_default_environment(void)
{
	som_default_environment();
}

int board_late_init(void)
{
	const void *fdt_compat;
	int fdt_compat_len;
	char dtb_name[256];
	int buf_len;

	if (IS_ENABLED(CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG)) {
		fdt_compat = fdt_getprop(gd->fdt_blob, 0, "compatible",
					 &fdt_compat_len);
		if (fdt_compat && fdt_compat_len) {
			if (strncmp(fdt_compat, "st,", 3) != 0) {
				env_set("board_name", fdt_compat);
			} else {
				env_set("board_name", fdt_compat + 3);

				buf_len = sizeof(dtb_name);
				strncpy(dtb_name, fdt_compat + 3, buf_len);
				buf_len -= strlen(fdt_compat + 3);
				strncat(dtb_name, ".dtb", buf_len);
				env_set("fdtfile", dtb_name);
			}
		}
	}

	/* Set default dynamic variables */
	platform_default_environment();

	return 0;
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	fdt_fixup_ccmp2(blob);
	fdt_fixup_carrierboard(blob);
	fdt_copy_fixed_partitions(blob);

	return 0;
}

#if defined(CONFIG_USB_DWC3) && defined(CONFIG_CMD_STM32PROG_USB)
#include <dfu.h>
/*
 * TEMP: force USB BUS reset forced to false, because it is not supported
 *       in DWC3 USB driver
 * avoid USB bus reset support in DFU stack is required to reenumeration in
 * stm32prog command after flashlayout load or after "dfu-util -e -R"
 */
bool dfu_usb_get_reset(void)
{
	return false;
}
#endif

/* weak function called from common/board_r.c */
int is_flash_available(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(stm32_hyperbus),
					  &dev);
	if (ret)
		return 0;

	return 1;
}

/* weak function called from env/sf.c */
void *env_sf_get_env_addr(void)
{
	return NULL;
}
