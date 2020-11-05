/*
 * Copyright (C) 2018-2020 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_esdhc_imx.h>

#include <asm/gpio.h>
#include <asm/arch/imx8-pins.h>
#include <usb.h>
#include <asm/arch/iomux.h>
#include <asm/arch/sci/sci.h>
#include <asm/arch/snvs_security_sc.h>
#include <asm/arch/sys_proto.h>
#include <power-domain.h>
#include "../../freescale/common/tcpc.h"

#include "../ccimx8/ccimx8.h"
#include "../common/carrier_board.h"
#include "../common/helper.h"
#include "../common/mca_registers.h"
#include "../common/mca.h"
#include "../common/trustfence.h"

DECLARE_GLOBAL_DATA_PTR;

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

#define ESDHC_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ESDHC_CLK_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ENET_INPUT_PAD_CTRL	((SC_PAD_CONFIG_OD_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define ENET_NORMAL_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_18V_10MA << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define FSPI_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define GPIO_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define I2C_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_LOW << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define UART_PAD_CTRL	((SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) \
						| (SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static iomux_cfg_t uart2_pads[] = {
	SC_P_UART2_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	SC_P_UART2_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
#define GPI_PAD_CTRL	((SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
			(SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
			(SC_PAD_28FDSOI_DSE_DV_HIGH << PADRING_DSE_SHIFT) | \
			(SC_PAD_28FDSOI_PS_PD << PADRING_PULL_SHIFT))

static iomux_cfg_t const ext_gpios_pads[] = {
	SC_P_USDHC1_WP | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPI_PAD_CTRL),		/* GPIO4_IO21 */
	SC_P_USDHC1_VSELECT | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPI_PAD_CTRL),	/* GPIO4_IO20 */
	SC_P_USDHC1_RESET_B | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPI_PAD_CTRL),	/* GPIO4_IO19 */
	SC_P_ENET0_REFCLK_125M_25M | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPI_PAD_CTRL),/* GPIO5_IO09 */
};
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */

static void setup_iomux_uart(void)
{
	imx8_iomux_setup_multiple_pads(uart2_pads, ARRAY_SIZE(uart2_pads));
}

int board_early_init_f(void)
{
	int ret;
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
	const char *ext_gpios[] = {
		"GPIO4_21",	/* A7 */
		"GPIO4_20",	/* B7 */
		"GPIO4_19",	/* C15 */
		"GPIO5_9",	/* D19 */
	};
	const char *ext_gpio_name = ext_gpios[CONFIG_CONSOLE_ENABLE_GPIO_NR];
	imx8_iomux_setup_multiple_pads(ext_gpios_pads,
				       ARRAY_SIZE(ext_gpios_pads));
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */

	/* Power up UART2 */
	ret = sc_pm_set_resource_power_mode(-1, SC_R_UART_2, SC_PM_PW_MODE_ON);
	if (ret)
		return ret;

	/* Set UART2 clock root to 80 MHz */
	sc_pm_clock_rate_t rate = 80000000;
	ret = sc_pm_set_clock_rate(-1, SC_R_UART_2, 2, &rate);
	if (ret)
		return ret;

	/* Enable UART2 clock root */
	ret = sc_pm_clock_enable(-1, SC_R_UART_2, 2, true, false);
	if (ret)
		return ret;

	setup_iomux_uart();

#if defined(CONFIG_HAS_TRUSTFENCE) && defined(CONFIG_ENV_AES_CAAM_KEY)
	sc_pm_set_resource_power_mode(-1, SC_R_CAAM_JR3, SC_PM_PW_MODE_ON);
	sc_pm_set_resource_power_mode(-1, SC_R_CAAM_JR3_OUT, SC_PM_PW_MODE_ON);
#endif

#ifdef CONFIG_CONSOLE_DISABLE
	gd->flags |= (GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#ifdef CONFIG_CONSOLE_ENABLE_GPIO
	if (console_enable_gpio(ext_gpio_name))
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */
#endif /* CONFIG_CONSOLE_DISABLE */

	return 0;
}

#ifdef CONFIG_FEC_MXC
#include <miiphy.h>

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
#endif

#ifdef CONFIG_MXC_GPIO
static iomux_cfg_t board_gpios[] = {
};

static void board_gpio_init(void)
{
	imx8_iomux_setup_multiple_pads(board_gpios, ARRAY_SIZE(board_gpios));
}
#endif

int checkboard(void)
{
	board_version = get_carrierboard_version();
	board_id = get_carrierboard_id();

	print_som_info();
	print_carrierboard_info();
	print_bootinfo();

#ifdef SCI_FORCE_ABORT
	sc_rpc_msg_t abort_msg;

	puts("Send abort request\n");
	RPC_SIZE(&abort_msg) = 1;
	RPC_SVC(&abort_msg) = SC_RPC_SVC_ABORT;
	sc_ipc_write(-1, &abort_msg);

#endif /* SCI_FORCE_ABORT */

	return 0;
}

#ifdef CONFIG_USB

#ifdef CONFIG_USB_TCPC
#define USB_TYPEC_SEL IMX_GPIO_NR(4, 6)
#define USB_TYPEC_EN IMX_GPIO_NR(4, 5)

static iomux_cfg_t ss_gpios[] = {
	SC_P_USB_SS3_TC3 | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
	SC_P_USB_SS3_TC2 | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL),
};

struct tcpc_port port;
struct tcpc_port_config port_config = {
	.i2c_bus = 3,
	.addr = 0x52,
	.port_type = TYPEC_PORT_DFP,
};

void ss_mux_select(enum typec_cc_polarity pol)
{
	if (pol == TYPEC_POLARITY_CC1)
		gpio_direction_output(USB_TYPEC_SEL, 0);
	else
		gpio_direction_output(USB_TYPEC_SEL, 1);
}

static void setup_typec(void)
{
	imx8_iomux_setup_multiple_pads(ss_gpios, ARRAY_SIZE(ss_gpios));
	gpio_request(USB_TYPEC_SEL, "typec_sel");
	/* USB_SS3_SW_PWR (Active LOW) */
	gpio_request(USB_TYPEC_EN, "typec_en");
	gpio_direction_output(USB_TYPEC_EN, 0);

	tcpc_init(&port, port_config, &ss_mux_select);
}
#endif

static iomux_cfg_t usb_hub_gpio = {
	SC_P_SPI3_SDI | MUX_MODE_ALT(4) | MUX_PAD_CTRL(GPIO_PAD_CTRL)
};

static int setup_usb_hub(void)
{
	struct gpio_desc usb_pwr;
	int ret;

	imx8_iomux_setup_pad(usb_hub_gpio);

	/* Power up the USB hub */
	ret = dm_gpio_lookup_name("gpio0_15", &usb_pwr);
	if (ret)
		return -1;

	ret = dm_gpio_request(&usb_pwr, "usb_pwr");
	if (ret)
		return -1;

	dm_gpio_set_dir_flags(&usb_pwr, GPIOD_IS_OUT);
	dm_gpio_set_value(&usb_pwr, 1);

	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_dfp_mode(&port);
#endif
		} else {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_setup_ufp_mode(&port);
#endif
		}
	}

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	if (index == 1) {
		if (init == USB_INIT_HOST) {
#ifdef CONFIG_USB_TCPC
			ret = tcpc_disable_src_vbus(&port);
#endif
		}
	}

	return ret;
}
#endif

int board_power_led_init(void)
{
	/* MCA_IO13 (bank 1, bit 5) is connected to POWER_LED */
	int prw_led_gpiobank = 1;
	int pwr_led_gpiobit = (1 << 5);
	int ret;

	/* Configure as output */
	ret = mca_update_bits(MCA_GPIO_DIR_0 + prw_led_gpiobank, pwr_led_gpiobit, pwr_led_gpiobit);
	if (ret != 0)
		return ret;

	/* Turn on POWER_LED (high) */
	ret = mca_update_bits(MCA_GPIO_DATA_0 + prw_led_gpiobank, pwr_led_gpiobit, pwr_led_gpiobit);
	return ret;
}

int board_init(void)
{
	/* SOM init */
	ccimx8_init();

	board_power_led_init();

#ifdef CONFIG_MXC_GPIO
	board_gpio_init();
#endif

#if defined(CONFIG_USB)
	setup_usb_hub();
#if defined(CONFIG_USB_TCPC)
	setup_typec();
#endif
#endif

#ifdef CONFIG_SNVS_SEC_SC_AUTO
{
	int ret = snvs_security_sc_init();

	if (ret)
		return ret;
}
#endif

	return 0;
}

void board_quiesce_devices(void)
{
	const char *power_on_devices[] = {
		"dma_lpuart2",
		"PD_UART2_TX",

		/* HIFI DSP boot */
		"audio_sai0",
		"audio_ocram",
	};

	power_off_pd_devices(power_on_devices, ARRAY_SIZE(power_on_devices));
}

#if defined(CONFIG_OF_BOARD_SETUP)
/* Platform function to modify the FDT as needed */
int ft_board_setup(void *blob, bd_t *bd)
{
	fdt_fixup_ccimx8(blob);
	fdt_fixup_ccimx8x(blob);
	fdt_fixup_carrierboard(blob);

	return 0;
}
#endif /* CONFIG_OF_BOARD_SETUP */

void platform_default_environment(void)
{
	som_default_environment();
}

int board_late_init(void)
{
	build_info();
	/* SOM late init */
	ccimx8_late_init();

	/* Set default dynamic variables */
	platform_default_environment();

	return 0;
}
