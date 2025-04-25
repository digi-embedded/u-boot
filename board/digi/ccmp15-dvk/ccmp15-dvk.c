// SPDX-License-Identifier: GPL-2.0+ OR BSD-3-Clause
/*
 * Copyright (C) 2018, STMicroelectronics - All Rights Reserved
 * Copyright (C) 2022, Digi International Inc - All Rights Reserved
 */

#define LOG_CATEGORY LOGC_BOARD

#include <common.h>
#include <bootm.h>
#include <clk.h>
#include <config.h>
#include <dm.h>
#include <env.h>
#include <env_internal.h>
#include <fdt_simplefb.h>
#include <fdt_support.h>
#include <g_dnl.h>
#include <generic-phy.h>
#include <hang.h>
#include <i2c.h>
#include <regmap.h>
#include <init.h>
#include <led.h>
#include <log.h>
#include <malloc.h>
#include <misc.h>
#include <mtd_node.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <remoteproc.h>
#include <reset.h>
#include <syscon.h>
#include <watchdog.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/stm32.h>
#include <asm/arch/sys_proto.h>
#include <configs/digi_common.h>
#include <dm/device.h>
#include <dm/device-internal.h>
#include <jffs2/load_kernel.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/iopoll.h>
#include <power/regulator.h>
#include <tee/optee.h>

#include "../ccmp1/ccmp1.h"
#include "../common/carrier_board.h"

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

/* SYSCFG registers */
#define SYSCFG_BOOTR		0x00
#define SYSCFG_PMCSETR		0x04
#define SYSCFG_IOCTRLSETR	0x18
#define SYSCFG_ICNR		0x1C
#define SYSCFG_CMPCR		0x20
#define SYSCFG_CMPENSETR	0x24

#define SYSCFG_BOOTR_BOOT_MASK		GENMASK(2, 0)
#define SYSCFG_BOOTR_BOOTPD_SHIFT	4

#define SYSCFG_IOCTRLSETR_HSLVEN_TRACE		BIT(0)
#define SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI	BIT(1)
#define SYSCFG_IOCTRLSETR_HSLVEN_ETH		BIT(2)
#define SYSCFG_IOCTRLSETR_HSLVEN_SDMMC		BIT(3)
#define SYSCFG_IOCTRLSETR_HSLVEN_SPI		BIT(4)

#define SYSCFG_CMPCR_SW_CTRL		BIT(1)
#define SYSCFG_CMPCR_READY		BIT(8)

#define SYSCFG_CMPENSETR_MPU_EN		BIT(0)

#define SYSCFG_PMCSETR_ETH_CLK_SEL	BIT(16)
#define SYSCFG_PMCSETR_ETH_REF_CLK_SEL	BIT(17)

#define SYSCFG_PMCSETR_ETH_SELMII	BIT(20)

#define SYSCFG_PMCSETR_ETH_SEL_MASK	GENMASK(23, 21)
#define SYSCFG_PMCSETR_ETH_SEL_GMII_MII	0
#define SYSCFG_PMCSETR_ETH_SEL_RGMII	BIT(21)
#define SYSCFG_PMCSETR_ETH_SEL_RMII	BIT(23)

/*
 * Get a global data pointer
 */
DECLARE_GLOBAL_DATA_PTR;

#define USB_LOW_THRESHOLD_UV		200000
#define USB_WARNING_LOW_THRESHOLD_UV	660000
#define USB_START_LOW_THRESHOLD_UV	1230000
#define USB_START_HIGH_THRESHOLD_UV	2150000

int board_early_init_f(void)
{
	/* nothing to do, only used in SPL */
	return 0;
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

#ifdef CONFIG_USB_GADGET_DOWNLOAD
#define STM32MP1_G_DNL_DFU_PRODUCT_NUM 0xdf11
#define STM32MP1_G_DNL_FASTBOOT_PRODUCT_NUM 0x0afb

int g_dnl_bind_fixup(struct usb_device_descriptor *dev, const char *name)
{
	if (IS_ENABLED(CONFIG_DFU_OVER_USB) &&
	    !strcmp(name, "usb_dnl_dfu"))
		put_unaligned(STM32MP1_G_DNL_DFU_PRODUCT_NUM, &dev->idProduct);
	else if (IS_ENABLED(CONFIG_FASTBOOT) &&
		 !strcmp(name, "usb_dnl_fastboot"))
		put_unaligned(STM32MP1_G_DNL_FASTBOOT_PRODUCT_NUM,
			      &dev->idProduct);
	else
		put_unaligned(CONFIG_USB_GADGET_PRODUCT_NUM, &dev->idProduct);

	return 0;
}
#endif /* CONFIG_USB_GADGET_DOWNLOAD */

static int get_led(struct udevice **dev, char *led_string)
{
	char *led_name;
	int ret;

	led_name = fdtdec_get_config_string(gd->fdt_blob, led_string);
	if (!led_name) {
		log_debug("could not find %s config string\n", led_string);
		return -ENOENT;
	}
	ret = led_get_by_label(led_name, dev);
	if (ret) {
		log_debug("get=%d\n", ret);
		return ret;
	}

	return 0;
}

static int setup_led(enum led_state_t cmd)
{
	struct udevice *dev;
	int ret;

	if (!CONFIG_IS_ENABLED(LED))
		return 0;

	ret = get_led(&dev, "u-boot,boot-led");
	if (ret)
		return ret;

	ret = led_set_state(dev, cmd);
	return ret;
}

static void __maybe_unused led_error_blink(u32 nb_blink)
{
	int ret;
	struct udevice *led;
	u32 i;

	if (!nb_blink)
		return;

	if (CONFIG_IS_ENABLED(LED)) {
		ret = get_led(&led, "u-boot,error-led");
		if (!ret) {
			/* make u-boot,error-led blinking */
			/* if U32_MAX and 125ms interval, for 17.02 years */
			for (i = 0; i < 2 * nb_blink; i++) {
				led_set_state(led, LEDST_TOGGLE);
				mdelay(125);
				WATCHDOG_RESET();
			}
			led_set_state(led, LEDST_ON);
		}
	}

	/* infinite: the boot process must be stopped */
	if (nb_blink == U32_MAX)
		hang();
}

static void sysconf_init(void)
{
	u8 *syscfg;
	struct udevice *pwr_dev;
	struct udevice *pwr_reg;
	struct udevice *dev;
	u32 otp = 0;
	int ret;
	u32 bootr, val;

	syscfg = (u8 *)syscon_get_first_range(STM32MP_SYSCON_SYSCFG);

	/* interconnect update : select master using the port 1 */
	/* LTDC = AXI_M9 */
	/* GPU  = AXI_M8 */
	/* today information is hardcoded in U-Boot */
	writel(BIT(9), syscfg + SYSCFG_ICNR);

	/* disable Pull-Down for boot pin connected to VDD */
	bootr = readl(syscfg + SYSCFG_BOOTR);
	bootr &= ~(SYSCFG_BOOTR_BOOT_MASK << SYSCFG_BOOTR_BOOTPD_SHIFT);
	bootr |= (bootr & SYSCFG_BOOTR_BOOT_MASK) << SYSCFG_BOOTR_BOOTPD_SHIFT;
	writel(bootr, syscfg + SYSCFG_BOOTR);

	/* High Speed Low Voltage Pad mode Enable for SPI, SDMMC, ETH, QSPI
	 * and TRACE. Needed above ~50MHz and conditioned by AFMUX selection.
	 * The customer will have to disable this for low frequencies
	 * or if AFMUX is selected but the function not used, typically for
	 * TRACE. Otherwise, impact on power consumption.
	 *
	 * WARNING:
	 *   enabling High Speed mode while VDD>2.7V
	 *   with the OTP product_below_2v5 (OTP 18, BIT 13)
	 *   erroneously set to 1 can damage the IC!
	 *   => U-Boot set the register only if VDD < 2.7V (in DT)
	 *      but this value need to be consistent with board design
	 */
	ret = uclass_get_device_by_driver(UCLASS_PMIC,
					  DM_DRIVER_GET(stm32mp_pwr_pmic),
					  &pwr_dev);
	if (!ret && IS_ENABLED(CONFIG_DM_REGULATOR)) {
		ret = uclass_get_device_by_driver(UCLASS_MISC,
						  DM_DRIVER_GET(stm32mp_bsec),
						  &dev);
		if (ret) {
			log_err("Can't find stm32mp_bsec driver\n");
			return;
		}

		ret = misc_read(dev, STM32_BSEC_SHADOW(18), &otp, 4);
		if (ret > 0)
			otp = otp & BIT(13);

		/* get VDD = vdd-supply */
		ret = device_get_supply_regulator(pwr_dev, "vdd-supply",
						  &pwr_reg);

		/* check if VDD is Low Voltage */
		if (!ret) {
			if (regulator_get_value(pwr_reg) < 2700000) {
				writel(SYSCFG_IOCTRLSETR_HSLVEN_TRACE |
				       SYSCFG_IOCTRLSETR_HSLVEN_QUADSPI |
				       SYSCFG_IOCTRLSETR_HSLVEN_ETH |
				       SYSCFG_IOCTRLSETR_HSLVEN_SDMMC |
				       SYSCFG_IOCTRLSETR_HSLVEN_SPI,
				       syscfg + SYSCFG_IOCTRLSETR);

				if (!otp)
					log_err("product_below_2v5=0: HSLVEN protected by HW\n");
			} else {
				if (otp)
					log_err("product_below_2v5=1: HSLVEN update is destructive, no update as VDD>2.7V\n");
			}
		} else {
			log_debug("VDD unknown");
		}
	}

	/* activate automatic I/O compensation
	 * warning: need to ensure CSI enabled and ready in clock driver
	 */
	writel(SYSCFG_CMPENSETR_MPU_EN, syscfg + SYSCFG_CMPENSETR);

	/* poll until ready (1s timeout) */
	ret = readl_poll_timeout(syscfg + SYSCFG_CMPCR, val,
				 val & SYSCFG_CMPCR_READY,
				 1000000);
	if (ret) {
		log_err("SYSCFG: I/O compensation failed, timeout.\n");
		led_error_blink(10);
	}

	clrbits_le32(syscfg + SYSCFG_CMPCR, SYSCFG_CMPCR_SW_CTRL);
}

/* board dependent setup after realloc */
int board_init(void)
{
	struct udevice *dev;
	int ret;

	/* probe RCC to avoid circular access with usbphyc probe as clk provider */
	if (IS_ENABLED(CONFIG_CLK_STM32MP13)) {
		ret = uclass_get_device_by_driver(UCLASS_CLK, DM_DRIVER_GET(stm32mp1_clock), &dev);
		log_debug("Clock init failed: %d\n", ret);
	}

	/* address of boot parameters */
	gd->bd->bi_boot_params = STM32_DDR_BASE + 0x100;

	if (IS_ENABLED(CONFIG_DM_REGULATOR))
		regulators_enable_boot_on(_DEBUG);

	/*
	 * sysconf initialisation done only when U-Boot is running in secure
	 * done in TF-A for TFABOOT.
	 */
	if (IS_ENABLED(CONFIG_ARMV7_NONSEC))
		sysconf_init();

	if (CONFIG_IS_ENABLED(LED))
		led_default_state();

	setup_led(LEDST_ON);

	/* SOM init */
	ccmp1_init();

#ifdef CONFIG_CONSOLE_ENABLE_GPIO
	struct gpio_desc desc;
	const char *ext_gpio_name = CONFIG_CONSOLE_ENABLE_GPIO_NAME;
	ret = -1;

	if (dm_gpio_lookup_name(ext_gpio_name, &desc))
		goto error;

	if (dm_gpio_request(&desc, "Console enable"))
		goto error_free;

	if (dm_gpio_set_dir_flags(&desc, GPIOD_IS_IN))
		goto error_free;

	ret = dm_gpio_get_value(&desc);
	if ( ret )
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);

	ret = 0;

error_free:
	dm_gpio_free(NULL, &desc);
error:
	return ret;
#endif /* CONFIG_CONSOLE_ENABLE_GPIO */
	return 0;
}

void platform_default_environment(void)
{
	som_default_environment();
}

int board_late_init(void)
{
	/* Set default dynamic variables */
	platform_default_environment();

	return 0;
}

void board_quiesce_devices(void)
{
	setup_led(LEDST_OFF);
}

/* CLOCK feed to PHY*/
#define ETH_CK_F_25M	25000000
#define ETH_CK_F_50M	50000000
#define ETH_CK_F_125M	125000000

struct stm32_syscfg_pmcsetr {
	u32 syscfg_clr_off;
	u32 eth1_clk_sel;
	u32 eth1_ref_clk_sel;
	u32 eth1_sel_mii;
	u32 eth1_sel_rgmii;
	u32 eth1_sel_rmii;
	u32 eth2_clk_sel;
	u32 eth2_ref_clk_sel;
	u32 eth2_sel_rgmii;
	u32 eth2_sel_rmii;
};

const struct stm32_syscfg_pmcsetr stm32mp15_syscfg_pmcsetr = {
	.syscfg_clr_off		= 0x44,
	.eth1_clk_sel		= BIT(16),
	.eth1_ref_clk_sel	= BIT(17),
	.eth1_sel_mii		= BIT(20),
	.eth1_sel_rgmii		= BIT(21),
	.eth1_sel_rmii		= BIT(23),
	.eth2_clk_sel		= 0,
	.eth2_ref_clk_sel	= 0,
	.eth2_sel_rgmii		= 0,
	.eth2_sel_rmii		= 0
};

const struct stm32_syscfg_pmcsetr stm32mp13_syscfg_pmcsetr = {
	.syscfg_clr_off		= 0x08,
	.eth1_clk_sel		= BIT(16),
	.eth1_ref_clk_sel	= BIT(17),
	.eth1_sel_mii		= 0,
	.eth1_sel_rgmii		= BIT(21),
	.eth1_sel_rmii		= BIT(23),
	.eth2_clk_sel		= BIT(24),
	.eth2_ref_clk_sel	= BIT(25),
	.eth2_sel_rgmii		= BIT(29),
	.eth2_sel_rmii		= BIT(31)
};

#define SYSCFG_PMCSETR_ETH_MASK		GENMASK(23, 16)
#define SYSCFG_PMCR_ETH_SEL_GMII	0

/* eth init function : weak called in eqos driver */
int board_interface_eth_init(struct udevice *dev,
			     phy_interface_t interface_type, ulong rate)
{
	struct regmap *regmap;
	uint regmap_mask;
	int ret;
	u32 value;
	bool ext_phyclk, eth_clk_sel_reg, eth_ref_clk_sel_reg;
	const struct stm32_syscfg_pmcsetr *pmcsetr;

	/* Ethernet PHY have no crystal */
	ext_phyclk = dev_read_bool(dev, "st,ext-phyclk");

	/* Gigabit Ethernet 125MHz clock selection. */
	eth_clk_sel_reg = dev_read_bool(dev, "st,eth-clk-sel");

	/* Ethernet 50Mhz RMII clock selection */
	eth_ref_clk_sel_reg = dev_read_bool(dev, "st,eth-ref-clk-sel");

	if (device_is_compatible(dev, "st,stm32mp13-dwmac"))
		pmcsetr = &stm32mp13_syscfg_pmcsetr;
	else
		pmcsetr = &stm32mp15_syscfg_pmcsetr;

	regmap = syscon_regmap_lookup_by_phandle(dev, "st,syscon");
	if (!IS_ERR(regmap)) {
		u32 fmp[3];

		ret = dev_read_u32_array(dev, "st,syscon", fmp, 3);
		if (ret)
			/*  If no mask in DT, it is MP15 (backward compatibility) */
			regmap_mask = SYSCFG_PMCSETR_ETH_MASK;
		else
			regmap_mask = fmp[2];
	} else {
		return -ENODEV;
	}

	switch (interface_type) {
	case PHY_INTERFACE_MODE_MII:
		value = pmcsetr->eth1_sel_mii;
		log_debug("PHY_INTERFACE_MODE_MII\n");
		break;
	case PHY_INTERFACE_MODE_GMII:
		value = SYSCFG_PMCR_ETH_SEL_GMII;
		log_debug("PHY_INTERFACE_MODE_GMII\n");
		break;
	case PHY_INTERFACE_MODE_RMII:
		value = pmcsetr->eth1_sel_rmii | pmcsetr->eth2_sel_rmii;
		if (rate == ETH_CK_F_50M && (eth_clk_sel_reg || ext_phyclk))
			value |= pmcsetr->eth1_ref_clk_sel | pmcsetr->eth2_ref_clk_sel;
		log_debug("PHY_INTERFACE_MODE_RMII\n");
		break;
	case PHY_INTERFACE_MODE_RGMII:
	case PHY_INTERFACE_MODE_RGMII_ID:
	case PHY_INTERFACE_MODE_RGMII_RXID:
	case PHY_INTERFACE_MODE_RGMII_TXID:
		value = pmcsetr->eth1_sel_rgmii | pmcsetr->eth2_sel_rgmii;
		if (rate == ETH_CK_F_125M && (eth_clk_sel_reg || ext_phyclk))
			value |= pmcsetr->eth1_clk_sel | pmcsetr->eth2_clk_sel;
		log_debug("PHY_INTERFACE_MODE_RGMII\n");
		break;
	default:
		log_debug("Do not manage %d interface\n",
			  interface_type);
		/* Do not manage others interfaces */
		return -EINVAL;
	}

	/* Need to update PMCCLRR (clear register) */
	regmap_write(regmap, pmcsetr->syscfg_clr_off, regmap_mask);

	ret = regmap_update_bits(regmap, SYSCFG_PMCSETR, regmap_mask, value);

	return ret;
}

#if defined(CONFIG_OF_BOARD_SETUP)

/* update scmi nodes with information provided by SP-MIN */
void stm32mp15_fdt_update_scmi_node(void *new_blob)
{
	ofnode node;
	int nodeoff = 0;
	const char *name;
	u32 val;
	int ret;

	nodeoff = fdt_path_offset(new_blob, "/firmware/scmi");
	if (nodeoff < 0)
		return;

	/* search scmi node in U-Boot device tree */
	node = ofnode_path("/firmware/scmi");
	if (!ofnode_valid(node)) {
		log_warning("node not found");
		return;
	}
	if (!ofnode_device_is_compatible(node, "arm,scmi-smc")) {
		name = ofnode_get_property(node, "compatible", NULL);
		log_warning("invalid compatible %s", name);
		return;
	}

	/* read values updated by TF-A SP-MIN */
	ret = ofnode_read_u32(node, "arm,smc-id", &val);
	if (ret) {
		log_warning("arm,smc-id missing");
		return;
	}
	/* update kernel node */
	fdt_setprop_string(new_blob, nodeoff, "compatible", "arm,scmi-smc");
	fdt_delprop(new_blob, nodeoff, "linaro,optee-channel-id");
	fdt_setprop_u32(new_blob, nodeoff, "arm,smc-id", val);
}

/*
 * update the device tree to support boot with SP-MIN, using a device tree
 * containing OPTE nodes:
 * 1/ remove the OP-TEE related nodes
 * 2/ copy SCMI nodes to kernel device tree to replace the OP-TEE agent
 *
 * SP-MIN boot is supported for STM32MP15 and it uses the SCMI SMC agent
 * whereas Linux device tree defines an SCMI OP-TEE agent.
 *
 * This function allows to temporary support this legacy boot mode,
 * with SP-MIN and without OP-TEE.
 */
void stm32mp15_fdt_update_optee_nodes(void *new_blob)
{
	ofnode node;
	int nodeoff = 0, subnodeoff;

	/* only proceed if /firmware/optee node is not present in U-Boot DT */
	node = ofnode_path("/firmware/optee");
	if (ofnode_valid(node)) {
		log_debug("OP-TEE firmware found, nothing to do");
		return;
	}

	/* remove OP-TEE memory regions in reserved-memory node */
	nodeoff = fdt_path_offset(new_blob, "/reserved-memory");
	if (nodeoff >= 0) {
		fdt_for_each_subnode(subnodeoff, new_blob, nodeoff) {
			const char *name = fdt_get_name(new_blob, subnodeoff, NULL);

			/* only handle "optee" reservations */
			if (name && !strncmp(name, "optee", 5))
				fdt_del_node(new_blob, subnodeoff);
		}
	}

	/* remove OP-TEE node  */
	nodeoff = fdt_path_offset(new_blob, "/firmware/optee");
	if (nodeoff >= 0)
		fdt_del_node(new_blob, nodeoff);

	/* update the scmi node */
	stm32mp15_fdt_update_scmi_node(new_blob);
}


void fdt_update_panel_dsi(void *new_blob)
{
	char const *panel = env_get("panel-dsi");
	int nodeoff = 0;

	if (!panel)
		return;

	if (!strcmp(panel, "rocktech,hx8394")) {
		nodeoff = fdt_node_offset_by_compatible(new_blob, -1, "raydium,rm68200");
		if (nodeoff < 0) {
			log_warning("panel-dsi node not found");
			return;
		}
		fdt_setprop_string(new_blob, nodeoff, "compatible", panel);

		nodeoff = fdt_node_offset_by_compatible(new_blob, -1, "goodix,gt9147");
		if (nodeoff < 0) {
			log_warning("touchscreen node not found");
			return;
		}
		fdt_setprop_string(new_blob, nodeoff, "compatible", "goodix,gt911");
	}
}

int ft_board_setup(void *blob, struct bd_info *bd)
{
	fdt_fixup_ccmp1(blob);
	fdt_fixup_carrierboard(blob);

	static const struct node_info nodes[] = {
		{ "st,stm32f469-qspi",		MTD_DEV_TYPE_NOR,  },
		{ "st,stm32f469-qspi",		MTD_DEV_TYPE_SPINAND},
		{ "st,stm32mp15-fmc2",		MTD_DEV_TYPE_NAND, },
		{ "st,stm32mp1-fmc2-nfc",	MTD_DEV_TYPE_NAND, },
	};

	if (IS_ENABLED(CONFIG_FDT_FIXUP_PARTITIONS))
		fdt_fixup_mtdparts(blob, nodes, ARRAY_SIZE(nodes));

	if (CONFIG_IS_ENABLED(FDT_SIMPLEFB))
		fdt_simplefb_enable_and_mem_rsv(blob);

	stm32mp15_fdt_update_optee_nodes(blob);
	return 0;
}
#endif

static void board_copro_image_process(ulong fw_image, size_t fw_size)
{
	int ret, id = 0; /* Copro id fixed to 0 as only one coproc on mp1 */

	if (!rproc_is_initialized())
		if (rproc_init()) {
			log_err("Remote Processor %d initialization failed\n",
				id);
			return;
		}

	ret = rproc_load(id, fw_image, fw_size);
	log_err("Load Remote Processor %d with data@addr=0x%08lx %u bytes:%s\n",
		id, fw_image, fw_size, ret ? " Failed!" : " Success!");

	if (!ret)
		rproc_start(id);
}

U_BOOT_FIT_LOADABLE_HANDLER(IH_TYPE_COPRO, board_copro_image_process);
