/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2013 Digi International, Inc.
 *
 * Author: Fabio Estevam <fabio.estevam@freescale.com>
 * Author: Jason Liu <r64343@freescale.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <common.h>
#include <asm/arch/iomux.h>
#include <asm/arch/mx6-pins.h>
#include <asm/gpio.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include <fsl_esdhc.h>
#include <fuse.h>
#include <micrel.h>
#include <miiphy.h>
#include <mmc.h>
#include <netdev.h>
#ifdef CONFIG_SYS_I2C_MXC
#include <i2c.h>
#include <asm/imx-common/mxc_i2c.h>
#endif
#include <asm/imx-common/boot_mode.h>
#include <asm/imx-common/iomux-v3.h>
#include "../ccimx6/ccimx6.h"
#include "../common/hwid.h"

DECLARE_GLOBAL_DATA_PTR;

static int phy_addr;

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |               \
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

iomux_v3_cfg_t const uart4_pads[] = {
	MX6_PAD_KEY_COL0__UART4_TX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_KEY_ROW0__UART4_RX_DATA | MUX_PAD_CTRL(UART_PAD_CTRL),
};

iomux_v3_cfg_t const ksz9031_pads[] = {
	/* Micrel KSZ9031 PHY reset */
	MX6_PAD_ENET_CRS_DV__GPIO1_IO25		| MUX_PAD_CTRL(NO_PAD_CTRL),
};

iomux_v3_cfg_t const sgtl5000_audio_pads[] = {
	/*
	 * Audio lines must be configured as GPIO inputs when coming
	 * from a software reset, since the audio chip itself does not have a
	 * reset line, and the codec might get power from I2S lines otherwise.
	 */
	MX6_PAD_CSI0_DAT7__GPIO5_IO25 | MUX_PAD_CTRL(NO_PAD_CTRL), /* RXD */
	MX6_PAD_CSI0_DAT4__GPIO5_IO22 | MUX_PAD_CTRL(NO_PAD_CTRL), /* TXC */
	MX6_PAD_CSI0_DAT5__GPIO5_IO23 | MUX_PAD_CTRL(NO_PAD_CTRL), /* TXD */
	MX6_PAD_CSI0_DAT6__GPIO5_IO24 | MUX_PAD_CTRL(NO_PAD_CTRL), /* TXFS */
};

iomux_v3_cfg_t const sgtl5000_pwr_pads[] = {
	/* SGTL5000 audio codec power enable (external 4K7 pull-up) */
	MX6_PAD_EIM_OE__GPIO2_IO25 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

unsigned int get_carrierboard_version(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	u32 version;

	if (fuse_read(CONFIG_CARRIERBOARD_VERSION_BANK,
		      CONFIG_CARRIERBOARD_VERSION_WORD, &version))
		return CARRIERBOARD_VERSION_UNDEFINED;

	version >>= CONFIG_CARRIERBOARD_VERSION_OFFSET;
	version &= CONFIG_CARRIERBOARD_VERSION_MASK;

	return((int)version);
#else
	return CARRIERBOARD_VERSION_UNDEFINED;
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */
}

unsigned int get_carrierboard_id(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_ID
	u32 id;

	if (fuse_read(CONFIG_CARRIERBOARD_ID_BANK,
		      CONFIG_CARRIERBOARD_ID_WORD, &id))
		return CARRIERBOARD_ID_UNDEFINED;

	id >>= CONFIG_CARRIERBOARD_ID_OFFSET;
	id &= CONFIG_CARRIERBOARD_ID_MASK;

	return((int)id);
#else
	return CARRIERBOARD_ID_UNDEFINED;
#endif /* CONFIG_HAS_CARRIERBOARD_ID */
}

#ifdef CONFIG_SYS_I2C_MXC
int setup_pmic_voltages_carrierboard(void)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_PMIC_I2C_BUS))
                return -1;
#endif

	if (i2c_probe(CONFIG_PMIC_I2C_ADDR)) {
		printf("ERR: cannot access the PMIC\n");
		return -1;
	}

#if defined(CONFIG_FEC_MXC)
	/* Both NVCC_ENET and NVCC_RGMII come from LDO4 (2.5V) */
	/* Config LDO4 voltages A and B at 2.5V, then enable VLDO4 */
	if (pmic_write_reg(DA9063_VLDO4_A_ADDR, 0x50) ||
	    pmic_write_reg(DA9063_VLDO4_B_ADDR, 0x50) ||
	    pmic_write_bitfield(DA9063_VLDO4_CONT_ADDR, 0x1, 0, 0x1))
		printf("Could not configure VLDO4\n");
#endif
	/* PMIC GPIO11 is the LVDS0 backlight which is low level
	 * enabled. If left with default configuration (input) or when
	 * coming from power-off, the backlight may be enabled and draw
	 * too much power from the 5V source when this source is
	 * enabled, which may cause a voltage drop on the 5V line and
	 * hang the I2C bus where the touch controller is attached.
	 * To prevent this, configure GPIO11 as output and set it
	 * high, to make sure the backlight is disabled when the 5V is
	 * enabled.
	 * This also configures it as active-low when acting as PWM.
	 */
	if (pmic_write_bitfield(DA9063_GPIO10_11_ADDR, 0x3, 4, 0x3))
		printf("Could not configure GPIO11\n");
	if (pmic_write_bitfield(DA9063_GPIO_MODE8_15_ADDR, 0x1, 3, 0x1))
		printf("Could not set GPIO11 high\n");

	/* Similarly, do the same with PMIC_GPIO15 (LVDS1 backlight)
	 * This also configures it as active-low when acting as PWM.
	 */
	if (pmic_write_bitfield(DA9063_GPIO14_15_ADDR, 0x3, 4, 0x3))
		printf("Could not configure GPIO11\n");
	if (pmic_write_bitfield(DA9063_GPIO_MODE8_15_ADDR, 0x1, 7, 0x1))
		printf("Could not set GPIO11 high\n");

	/* PWR_EN on the ccimx6sbc enables the +5V suppy and comes
	 * from PMIC_GPIO7. Set this GPIO high to enable +5V supply.
	 */
	if (pmic_write_bitfield(DA9063_GPIO6_7_ADDR, 0x3, 4, 0x3))
		printf("Could not configure GPIO7\n");
	if (pmic_write_bitfield(DA9063_GPIO_MODE0_7_ADDR, 0x1, 7, 0x1))
		printf("Could not enable PWR_EN\n");

	return 0;
}
#endif /* CONFIG_SYS_I2C_MXC */

static void setup_board_enet(void)
{
	int phy_reset_gpio;

	/* Gigabit ENET (Micrel PHY) */
	phy_reset_gpio = IMX_GPIO_NR(1, 25);
	phy_addr = CONFIG_ENET_PHYADDR_MICREL;
	imx_iomux_v3_setup_multiple_pads(ksz9031_pads,
					 ARRAY_SIZE(ksz9031_pads));
	/* Assert PHY reset */
	gpio_direction_output(phy_reset_gpio , 0);
	/* Need 10ms to guarantee stable voltages */
	udelay(10 * 1000);
	/* Deassert PHY reset */
	gpio_set_value(phy_reset_gpio, 1);
	/* Need to wait 100us before accessing the MIIM (MDC/MDIO) */
	udelay(100);
}

int board_get_enet_phy_addr(void)
{
	return phy_addr;
}

static int mx6_rgmii_rework(struct phy_device *phydev)
{
	char *phy_mode;

	/*
	 * Micrel PHY KSZ9031 has four MMD registers to configure the clock skew
	 * of different signals. In U-Boot we're having Ethernet issues on
	 * certain boards which work fine in Linux. We examined these MMD clock
	 * skew registers in Linux which have different values than the reset
	 * defaults:
	 * 			Reset default		Linux
	 * ------------------------------------------------------------------
	 *  Control data pad	0077 (no skew)		0000 (-0.42 ns)
	 *  RX data pad		7777 (no skew)		0000 (-0.42 ns)
	 *  TX data pad		7777 (no skew)		7777 (no skew)
	 *  Clock pad		3def (no skew)		03ff (+0.96 ns)
	 *
	 *  Setting the skews used in Linux solves the issues in U-Boot.
	 */

	/* control data pad skew - devaddr = 0x02, register = 0x04 */
	ksz9031_phy_extended_write(phydev, 0x02,
				   MII_KSZ9031_EXT_RGMII_CTRL_SIG_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x0000);
	/* rx data pad skew - devaddr = 0x02, register = 0x05 */
	ksz9031_phy_extended_write(phydev, 0x02,
				   MII_KSZ9031_EXT_RGMII_RX_DATA_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x0000);
	/* tx data pad skew - devaddr = 0x02, register = 0x05 */
	ksz9031_phy_extended_write(phydev, 0x02,
				   MII_KSZ9031_EXT_RGMII_TX_DATA_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x7777);
	/* gtx and rx clock pad skew - devaddr = 0x02, register = 0x08 */
	ksz9031_phy_extended_write(phydev, 0x02,
				   MII_KSZ9031_EXT_RGMII_CLOCK_SKEW,
				   MII_KSZ9031_MOD_DATA_NO_POST_INC, 0x03ff);

	phy_mode = getenv("phy_mode");
	if (!strcmp("master", phy_mode)) {
		unsigned short reg;

		/*
		 * Micrel PHY KSZ9031 takes up to 5 seconds to autonegotiate
		 * with Gigabit switches. This time can be reduced by forcing
		 * the PHY to work as master during master-slave negotiation.
		 * Forcing master mode may cause autonegotiation to fail if
		 * the other end is also forced as master, or using a direct
		 * cable connection.
		 */
		reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_CTRL1000);
		reg |= MSTSLV_MANCONFIG_ENABLE | MSTSLV_MANCONFIG_MASTER;
		phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000, reg);
	}

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	mx6_rgmii_rework(phydev);
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart4_pads, ARRAY_SIZE(uart4_pads));
}

int board_eth_init(bd_t *bis)
{
	setup_iomux_enet();
	setup_board_enet();

	return cpu_eth_init(bis);
}

static int board_has_audio(void)
{
	switch(get_carrierboard_id()) {
	case CCIMX6SBC_ID129:
	case CCIMX6SBC_ID130:
		return 1;
	default:
		return 0;
	}
}

static void setup_board_audio(void)
{
	unsigned int board_id = get_carrierboard_id();

	/*
	 * The codec does not have a reset line so after a reset the
	 * ADC may be active and spitting noise.
	 * Power it off by pulling the power enable line down (it is externally
	 * pulled-up, and thus audio codec is ON by default) and configuring
	 * the audio lines as GPIO inputs (the codec might get power from I2S
	 * lines otherwise).
	 */

	/* Audio lines IOMUX */
	imx_iomux_v3_setup_multiple_pads(sgtl5000_audio_pads,
					 ARRAY_SIZE(sgtl5000_audio_pads));

	/* SBC version 2 and later use a GPIO to power enable the audio codec */
	if (((board_id == CCIMX6SBC_ID129) || (board_id == CCIMX6SBC_ID130)) &&
	    get_carrierboard_version() >= 2) {
		int pwren_gpio = IMX_GPIO_NR(2, 25);

		/* Power enable line IOMUX */
		imx_iomux_v3_setup_multiple_pads(sgtl5000_pwr_pads,
						 ARRAY_SIZE(sgtl5000_pwr_pads));
		gpio_direction_output(pwren_gpio , 0);
	}
}

int board_mmc_getcd(struct mmc *mmc)
{
	struct fsl_esdhc_cfg *cfg = (struct fsl_esdhc_cfg *)mmc->priv;
	int ret = 0;

	switch (cfg->esdhc_base) {
	case USDHC2_BASE_ADDR:
		ret = 1; /* uSD/uSDHC2 does not connect CD. Assume present */
		break;
	case USDHC4_BASE_ADDR:
		if (board_has_emmc())
			ret = 1;
		break;
	}

	return ret;
}

int board_early_init_f(void)
{
	setup_iomux_uart();

	return 0;
}

int board_init(void)
{
	/* address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

	ccimx6_init();
#ifdef CONFIG_CMD_SATA
	setup_iomux_sata();
#endif
	if (board_has_audio())
		setup_board_audio();

	return 0;
}

static void fdt_fixup_carrierboard(void *fdt)
{
#if defined(CONFIG_HAS_CARRIERBOARD_VERSION) || \
    defined(CONFIG_HAS_CARRIERBOARD_ID)
	char str[20];
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	sprintf(str, "%d", get_carrierboard_version());
	do_fixup_by_path(fdt, "/", "digi,carrierboard,version", str,
			 strlen(str) + 1, 1);
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
	sprintf(str, "%d", get_carrierboard_id());
	do_fixup_by_path(fdt, "/", "digi,carrierboard,id", str,
			 strlen(str) + 1, 1);
#endif
}

static void print_carrierboard_info(void)
{
	int board_version = get_carrierboard_version();
	int board_id = get_carrierboard_id();

	printf("Board: %s\n", CONFIG_BOARD_DESCRIPTION);
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	if (CARRIERBOARD_VERSION_UNDEFINED == board_version)
		printf("       WARNING: Undefined board version!\n");
	else
		printf("       Version: %d\n", board_version);
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
	if (CARRIERBOARD_ID_UNDEFINED == board_id)
		printf("       WARNING: Undefined board ID!\n");
	else
		printf("       ID: %d\n", board_id);
#endif
}

int checkboard(void)
{
	print_ccimx6_info();
	print_carrierboard_info();
	printf("Boot device: %s\n", get_boot_device_name());
	return 0;
}

static int board_fixup(void)
{
	unsigned int board_version = get_carrierboard_version();

	/* Mask the CHG_WAKE interrupt. This pin should be grounded
	 * if unused. */
	if (pmic_write_bitfield(DA9063_IRQ_MASK_B_ADDR, 0x1, 0, 0x1)) {
		printf("Failed to mask CHG_WAKE. Spurious wake up events may occur\n");
		return -1;
	}

	if (board_version <= 1) {
		/* Mask the PMIC_GPIO7 interrupt which is N/C on the SBCv1. */
		if (pmic_write_bitfield(DA9063_GPIO6_7_ADDR, 0x1, 0x7, 0x1)) {
			printf("Failed to mask PMIC_GPIO7.\n");
			return -1;
		}
	}

	return 0;
}

int board_late_init(void)
{
	int ret;
	unsigned int board_version = get_carrierboard_version();
	unsigned int board_id = get_carrierboard_id();
	char cmd[80];

	/* Set $board_version variable if defined in OTP bits */
	if (board_version > 0) {
		sprintf(cmd, "setenv -f board_version %d", board_version);
		run_command(cmd, 0);
	}

	/* Set $board_id variable if defined in OTP bits */
	if (board_id > 0) {
		sprintf(cmd, "setenv -f board_id %d", board_id);
		run_command(cmd, 0);
	}

	ret = ccimx6_late_init();
	if (!ret)
		ret = board_fixup();

	return ret;
}

#if defined(CONFIG_OF_BOARD_SETUP)
/* Platform function to modify the FDT as needed */
int ft_board_setup(void *blob, bd_t *bd)
{

	/* Re-read HWID which could have been overriden by U-Boot commands */
	fdt_fixup_hwid(blob);

	fdt_fixup_carrierboard(blob);
	if (board_has_wireless())
		fdt_fixup_mac(blob, "wlanaddr", "/wireless");
	if (board_has_bluetooth())
		fdt_fixup_mac(blob, "btaddr", "/bluetooth");

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */
