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
#include <fsl_esdhc_imx.h>
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
#include "../../freescale/common/tcpc.h"
#include <usb.h>
#include <linux/ctype.h>

#include "../ccimx8/ccimx8.h"
#include "../common/carrier_board.h"
#include "../common/helper.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/tamper.h"
#include "../common/trustfence.h"

DECLARE_GLOBAL_DATA_PTR;

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)
#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_ODE | PAD_CTL_PUE | PAD_CTL_PE)

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MN_PAD_SAI2_RXC__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MN_PAD_SAI2_RXFS__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MN_PAD_GPIO1_IO02__WDOG1_WDOG_B  | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

#if defined(CONFIG_CONSOLE_ENABLE_GPIO) && !defined(CONFIG_SPL_BUILD)
#define GPI_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL2 | PAD_CTL_PE)

static iomux_v3_cfg_t const ext_gpios_pads[] = {
	IMX8MN_PAD_GPIO1_IO10__GPIO1_IO10 | MUX_PAD_CTRL(GPI_PAD_CTRL),
	IMX8MN_PAD_GPIO1_IO11__GPIO1_IO11 | MUX_PAD_CTRL(GPI_PAD_CTRL),
	IMX8MN_PAD_GPIO1_IO13__GPIO1_IO13 | MUX_PAD_CTRL(GPI_PAD_CTRL),
	IMX8MN_PAD_GPIO1_IO14__GPIO1_IO14 | MUX_PAD_CTRL(GPI_PAD_CTRL),
};
#endif /* CONFIG_CONSOLE_ENABLE_GPIO && !CONFIG_SPL_BUILD */

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;
#if defined(CONFIG_CONSOLE_ENABLE_GPIO) && !defined(CONFIG_SPL_BUILD)
	const char *ext_gpios[] = {
		"GPIO1_10",	/* J46.3 */
		"GPIO1_11",	/* J46.5 */
		"GPIO1_13",	/* J46.7 */
		"GPIO1_14",	/* J46.9 */
	};
	const char *ext_gpio_name = ext_gpios[CONFIG_CONSOLE_ENABLE_GPIO_NR];
	imx_iomux_v3_setup_multiple_pads(ext_gpios_pads,
					 ARRAY_SIZE(ext_gpios_pads));
#endif /* CONFIG_CONSOLE_ENABLE_GPIO && !CONFIG_SPL_BUILD */

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

#ifdef CONFIG_CONSOLE_DISABLE
	gd->flags |= (GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#if defined(CONFIG_CONSOLE_ENABLE_GPIO) && !defined(CONFIG_SPL_BUILD)
	if (console_enable_gpio(ext_gpio_name))
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif /* CONFIG_CONSOLE_ENABLE_GPIO && !CONFIG_SPL_BUILD */
#endif /* CONFIG_CONSOLE_DISABLE */

	return 0;
}

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	fdt_fixup_ccimx8(blob);
	fdt_fixup_carrierboard(blob);

	return 0;
}
#endif

void platform_default_environment(void)
{
	som_default_environment();
}

int board_late_init(void)
{
	/* SOM late init */
	ccimx8_late_init();

	/* Set default dynamic variables */
	platform_default_environment();

	return 0;
}

#ifdef CONFIG_FEC_MXC
static void enet_device_phy_reset(void)
{
	struct gpio_desc desc;
	struct udevice *dev = NULL;
	int ret;

	ret = dm_gpio_lookup_name("gpio5_3", &desc);
	if (ret)
		return;

	ret = dm_gpio_request(&desc, "fec1_reset");
	if (ret)
		return;

	dm_gpio_set_dir_flags(&desc, GPIOD_IS_OUT);
	dm_gpio_set_value(&desc, 0);
	udelay(50);
	dm_gpio_set_value(&desc, 1);
	dm_gpio_free(dev, &desc);

	udelay(10);
}

int board_phy_config(struct phy_device *phydev)
{
	/* Set RGMII IO voltage to 1.8V */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x1f);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x8);

	/* Introduce RGMII RX clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x00);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);

	/* Introduce RGMII TX clock delay */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x05);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x100);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_fec(void)
{
	struct iomuxc_gpr_base_regs *const iomuxc_gpr_regs
		= (struct iomuxc_gpr_base_regs *) IOMUXC_GPR_BASE_ADDR;
	struct gpio_desc enet_pwr;
	int ret;

	/* Power up the PHY */
	ret = dm_gpio_lookup_name("gpio5_4", &enet_pwr);
	if (ret)
		return -1;

	ret = dm_gpio_request(&enet_pwr, "fec1_pwr");
	if (ret)
		return -1;

	dm_gpio_set_dir_flags(&enet_pwr, GPIOD_IS_OUT);
	dm_gpio_set_value(&enet_pwr, 1);
	mdelay(1);	/* PHY power up time */

	/* Reset the PHY */
	enet_device_phy_reset();

	/* Use 125M anatop REF_CLK1 for ENET1, not from external */
	clrsetbits_le32(&iomuxc_gpr_regs->gpr[1],
			IOMUXC_GPR_GPR1_GPR_ENET1_TX_CLK_SEL_MASK, 0);
	return set_clk_enet(ENET_125MHZ);
}
#endif

int board_usb_init(int index, enum usb_init_type init)
{
	imx8m_usb_power(index, true);

	return 0;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	imx8m_usb_power(index, false);

	return 0;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	return USB_INIT_DEVICE;
}

int board_init(void)
{
	/* SOM init */
	ccimx8_init();

#ifdef CONFIG_MXC_SPI
	setup_spi();
#endif

#ifdef CONFIG_FEC_MXC
	setup_fec();
#endif

	return 0;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno + 1;
}

int checkboard(void)
{
	board_version = get_carrierboard_version();
	board_id = get_carrierboard_id();

	print_som_info();
	print_carrierboard_info();
	print_bootinfo();

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
