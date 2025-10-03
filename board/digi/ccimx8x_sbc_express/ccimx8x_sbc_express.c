/*
 * Copyright (C) 2018-2020 Digi International, Inc.
 * Copyright 2017 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <fsl_esdhc_imx.h>

#include <asm/global_data.h>
#include <asm/gpio.h>
#include <asm/arch/imx8-pins.h>
#include <usb.h>
#include <asm/arch/iomux.h>
#include <firmware/imx/sci/sci.h>
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

static void setup_iomux_uart(void)
{
	imx8_iomux_setup_multiple_pads(uart2_pads, ARRAY_SIZE(uart2_pads));
}

int board_early_init_r(void)
{
#if defined(CONFIG_HAS_TRUSTFENCE) && defined(CONFIG_CAAM_ENV_ENCRYPT)
	setup_caam();
#endif
	return 0;
}

int board_early_init_f(void)
{
	sc_pm_clock_rate_t rate = SC_80MHZ;
	int ret;

	/* Set UART2 clock root to 80 MHz */
	ret = sc_pm_setup_uart(SC_R_UART_2, rate);
	if (ret)
		return ret;

	setup_iomux_uart();

#ifdef CONFIG_CONSOLE_DISABLE
	gd->flags |= (GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#if defined(CONFIG_CONSOLE_ENABLE_GPIO)
	if (console_enable_gpio(CONFIG_CONSOLE_ENABLE_GPIO_NAME))
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */
#endif /* CONFIG_CONSOLE_DISABLE */

	return 0;
}

#if defined(CONFIG_DISPLAY_BOARDINFO_LATE)
/*
 * Call this during late initialization, after relocation and board setup,
 * as some initialization must be completed before printing the information.
 */
int checkboard(void)
{
	board_version = get_carrierboard_version();
	board_id = get_carrierboard_id();

	print_som_info();
	print_carrierboard_info();
	print_bootinfo();

	return 0;
}
#endif

#ifdef CONFIG_USB

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;

	return ret;

}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	return ret;
}
#endif

int board_init(void)
{
	/* SOM init */
	ccimx8_init();

#ifdef CONFIG_IMX_SNVS_SEC_SC_AUTO
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

	imx8_power_off_pd_devices(power_on_devices,
				  ARRAY_SIZE(power_on_devices));
}

#if defined(CONFIG_OF_BOARD_SETUP)
/* Platform function to modify the FDT as needed */
int ft_board_setup(void *blob, struct bd_info *bd)
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
