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
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <netdev.h>
#ifdef CONFIG_SYS_I2C_MXC
#include <i2c.h>
#include <asm/imx-common/mxc_i2c.h>
#endif
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

iomux_v3_cfg_t const sgtl5000_pads[] = {
	/* SGTL5000 audio codec power enable (external 4K7 pull-up) */
	MX6_PAD_EIM_OE__GPIO2_IO25 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

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
	/* PMIC GPIO11 is the LCD backlight which is low level
	 * enabled. If left with default configuration (input) or when
	 * coming from power-off, the backlight may be enabled and draw
	 * too much power from the 5V source when this source is
	 * enabled, which may cause a voltage drop on the 5V line and
	 * hang the I2C bus where the touch controller is attached.
	 * To prevent this, configure GPIO11 as output and set it
	 * high, to make sure the backlight is disabled when the 5V is
	 * enabled.
	 */
	if (pmic_write_bitfield(DA9063_GPIO10_11_ADDR, 0x3, 4, 0x3))
		printf("Could not configure GPIO11\n");
	if (pmic_write_bitfield(DA9063_GPIO_MODE8_15_ADDR, 0x1, 3, 0x1))
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

static void setup_board_audio(void)
{
	/* SBC version 2 uses a GPIO to power enable the audio codec */
	if (get_carrierboard_version() >= 2) {
		int pwren_gpio = IMX_GPIO_NR(2, 25);

		/* Power enable line IOMUX */
		imx_iomux_v3_setup_multiple_pads(sgtl5000_pads,
						 ARRAY_SIZE(sgtl5000_pads));
		/* The codec does not have a reset line so after a reset the
		 * codec may be active and spitting noise. Power it off by
		 * pulling the power enable line down (it is externally
		 * pulled-up, and thus audio codec is ON by default) */
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

#ifdef CONFIG_LDO_BYPASS_CHECK
void ldo_mode_set(int ldo_bypass)
{
	unsigned char value;
	int is_400M;
	unsigned char vddarm;

	/* increase VDDARM/VDDSOC to support 1.2G chip */
	if (check_1_2G()) {
		ldo_bypass = 0;	/* ldo_enable on 1.2G chip */
		printf("1.2G chip, increase VDDARM_IN/VDDSOC_IN\n");
		/* increase VDDARM to 1.425V */
		if (pmic_read_reg(DA9063_VBCORE1_A_ADDR, &value)) {
			printf("Read BCORE1 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x71;
		if (pmic_write_reg(DA9063_VBCORE1_A_ADDR, value)) {
			printf("Set BCORE1 error!\n");
			goto out;
		}

		/* increase VDDSOC to 1.425V */
		if (pmic_read_reg(DA9063_VBCORE2_A_ADDR, &value)) {
			printf("Read BCORE2 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x71;
		if (pmic_write_reg(DA9063_VBCORE2_A_ADDR, value)) {
			printf("Set BCORE2 error!\n");
			goto out;
		}
	}
	/* switch to ldo_bypass mode, boot on 800Mhz */
	if (ldo_bypass) {
		prep_anatop_bypass();

		/* decrease VDDARM for 400Mhz DQ:1.1V, DL:1.275V */
		if (pmic_read_reg(DA9063_VBCORE1_A_ADDR, &value)) {
			printf("Read BCORE1 error!\n");
			goto out;
		}
		value &= ~0x7f;
#if defined(CONFIG_MX6DL) || defined(CONFIG_MX6S)
		value |= 0x62;
#else
		value |= 0x50;
#endif
		if (pmic_write_reg(DA9063_VBCORE1_A_ADDR, value)) {
			printf("Set BCORE1 error!\n");
			goto out;
		}
		/* increase VDDSOC to 1.3V */
		if (pmic_read_reg(DA9063_VBCORE2_A_ADDR, &value)) {
			printf("Read BCORE2 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x64;
		if (pmic_write_reg(DA9063_VBCORE2_A_ADDR, value)) {
			printf("Set BCORE2 error!\n");
			goto out;
		}

		/*
		 * MX6Q:
		 * VDDARM:1.15V@800M; VDDSOC:1.175V@800M
		 * VDDARM:0.975V@400M; VDDSOC:1.175V@400M
		 * MX6DL:
		 * VDDARM:1.175V@800M; VDDSOC:1.175V@800M
		 * VDDARM:1.075V@400M; VDDSOC:1.175V@400M
		 */
		is_400M = set_anatop_bypass(0);
		if (is_400M)
#if defined(CONFIG_MX6DL) || defined(CONFIG_MX6S)
			vddarm = 0x4e;
#else
			vddarm = 0x43;
#endif
		else
#if defined(CONFIG_MX6DL) || defined(CONFIG_MX6S)
			vddarm = 0x57;
#else
			vddarm = 0x55;
#endif
		if (pmic_read_reg(DA9063_VBCORE1_A_ADDR, &value)) {
			printf("Read BCORE1 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= vddarm;
		if (pmic_write_reg(DA9063_VBCORE1_A_ADDR, value)) {
			printf("Set BCORE1 error!\n");
			goto out;
		}

		/* decrease VDDSOC to 1.175V */
		if (pmic_read_reg(DA9063_VBCORE2_A_ADDR, &value)) {
			printf("Read BCORE2 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x57;
		if (pmic_write_reg(DA9063_VBCORE2_A_ADDR, value)) {
			printf("Set BCORE2 error!\n");
			goto out;
		}

		finish_anatop_bypass();
		printf("switch to LDO bypass mode\n");
	}
	return;
out:
	printf("Error switching to LDO bypass mode\n");
}
#endif

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
	setup_board_audio();

	return 0;
}

static int board_fixup(void)
{
	unsigned int carrierboard_ver = get_carrierboard_version();

	/* Mask the CHG_WAKE interrupt. This pin should be grounded
	 * if unused. */
	if (pmic_write_bitfield(DA9063_IRQ_MASK_B_ADDR, 0x1, 0, 0x1)) {
		printf("Failed to mask CHG_WAKE. Spurious wake up events may occur\n");
		return -1;
	}


	if (carrierboard_ver <= 1) {
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

	ret = ccimx6_late_init();
	if (!ret)
		ret = board_fixup();

	return ret;
}
