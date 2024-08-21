// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 Digi International Inc
 */

#include <common.h>
#include <env.h>
#include <init.h>
#include <linux/bitfield.h>
#include <linux/delay.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/global_data.h>
#include <asm/arch-imx9/ccm_regs.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch-imx9/imx91_pins.h>
#include <asm/arch/clock.h>
#include <configs/digi_common.h>
#include <power/pmic.h>
#include "../../freescale/common/tcpc.h"
#include <dm/device.h>
#include <dm/uclass.h>
#include <usb.h>
#include <dwc3-uboot.h>

#include "../ccimx9/ccimx9.h"
#include "../common/carrier_board.h"
#include "../common/trustfence.h"

DECLARE_GLOBAL_DATA_PTR;

unsigned int board_version = CARRIERBOARD_VERSION_UNDEFINED;
unsigned int board_id = CARRIERBOARD_ID_UNDEFINED;

#define UART_PAD_CTRL	(PAD_CTL_DSE(6) | PAD_CTL_FSEL2)

static iomux_v3_cfg_t const uart_pads[] = {
	MX91_PAD_GPIO_IO05__LPUART6_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX91_PAD_GPIO_IO04__LPUART6_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

int board_early_init_f(void)
{
	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	init_uart_clk(LPUART6_CLK_ROOT);

	if (IS_ENABLED(CONFIG_CONSOLE_DISABLE))
		gd->flags |= (GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);

#if defined(CONFIG_CONSOLE_ENABLE_GPIO) && !defined(CONFIG_SPL_BUILD)
	if (console_enable_gpio(CONFIG_CONSOLE_ENABLE_GPIO_NAME))
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
#endif

	return 0;
}

#ifdef CONFIG_USB_TCPC
struct tcpc_port port1;
struct tcpc_port port2;
struct tcpc_port portpd;

static int setup_pd_switch(uint8_t i2c_bus, uint8_t addr)
{
	struct udevice *bus;
	struct udevice *i2c_dev = NULL;
	int ret;
	uint8_t valb;

	ret = uclass_get_device_by_seq(UCLASS_I2C, i2c_bus, &bus);
	if (ret) {
		printf("%s: Can't find bus\n", __func__);
		return -EINVAL;
	}

	ret = dm_i2c_probe(bus, addr, 0, &i2c_dev);
	if (ret) {
		printf("%s: Can't find device id=0x%x\n",
			__func__, addr);
		return -ENODEV;
	}

	ret = dm_i2c_read(i2c_dev, 0xB, &valb, 1);
	if (ret) {
		printf("%s dm_i2c_read failed, err %d\n", __func__, ret);
		return -EIO;
	}
	valb |= 0x4; /* Set DB_EXIT to exit dead battery mode */
	ret = dm_i2c_write(i2c_dev, 0xB, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	/* Set OVP threshold to 23V */
	valb = 0x6;
	ret = dm_i2c_write(i2c_dev, 0x8, (const uint8_t *)&valb, 1);
	if (ret) {
		printf("%s dm_i2c_write failed, err %d\n", __func__, ret);
		return -EIO;
	}

	return 0;
}

int pd_switch_snk_enable(struct tcpc_port *port)
{
	if (port == &port1) {
		debug("Setup pd switch on port 1\n");
		return setup_pd_switch(2, 0x71);
	} else if (port == &port2) {
		debug("Setup pd switch on port 2\n");
		return setup_pd_switch(2, 0x73);
	} else
		return -EINVAL;
}

struct tcpc_port_config portpd_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x52,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 20000,
	.max_snk_ma = 3000,
	.max_snk_mw = 15000,
	.op_snk_mv = 9000,
};

struct tcpc_port_config port1_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x50,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 5000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
	.disable_pd = true,
};

struct tcpc_port_config port2_config = {
	.i2c_bus = 2, /*i2c3*/
	.addr = 0x51,
	.port_type = TYPEC_PORT_UFP,
	.max_snk_mv = 9000,
	.max_snk_ma = 3000,
	.max_snk_mw = 40000,
	.op_snk_mv = 9000,
	.switch_setup_func = &pd_switch_snk_enable,
	.disable_pd = true,
};

static int setup_typec(void)
{
	int ret;

	debug("tcpc_init port pd\n");
	ret = tcpc_init(&portpd, portpd_config, NULL);
	if (ret) {
		printf("%s: tcpc portpd init failed, err=%d\n",
		       __func__, ret);
	}

	debug("tcpc_init port 2\n");
	ret = tcpc_init(&port2, port2_config, NULL);
	if (ret) {
		printf("%s: tcpc port2 init failed, err=%d\n",
		       __func__, ret);
	}

	debug("tcpc_init port 1\n");
	ret = tcpc_init(&port1, port1_config, NULL);
	if (ret) {
		printf("%s: tcpc port1 init failed, err=%d\n",
		       __func__, ret);
	}

	return ret;
}

int board_usb_init(int index, enum usb_init_type init)
{
	int ret = 0;
	struct tcpc_port *port_ptr;

	debug("board_usb_init %d, type %d\n", index, init);

	if (index == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	if (init == USB_INIT_HOST)
		tcpc_setup_dfp_mode(port_ptr);
	else
		tcpc_setup_ufp_mode(port_ptr);

	return ret;
}

int board_usb_cleanup(int index, enum usb_init_type init)
{
	int ret = 0;

	debug("board_usb_cleanup %d, type %d\n", index, init);

	if (init == USB_INIT_HOST) {
		if (index == 0)
			ret = tcpc_disable_src_vbus(&port1);
		else
			ret = tcpc_disable_src_vbus(&port2);
	}

	return ret;
}

int board_ehci_usb_phy_mode(struct udevice *dev)
{
	int ret = 0;
	enum typec_cc_polarity pol;
	enum typec_cc_state state;
	struct tcpc_port *port_ptr;

	debug("%s %d\n", __func__, dev_seq(dev));

	if (dev_seq(dev) == 0)
		port_ptr = &port1;
	else
		port_ptr = &port2;

	tcpc_setup_ufp_mode(port_ptr);

	ret = tcpc_get_cc_status(port_ptr, &pol, &state);

	tcpc_print_log(port_ptr);
	if (!ret) {
		if (state == TYPEC_STATE_SRC_RD_RA || state == TYPEC_STATE_SRC_RD)
			return USB_INIT_HOST;
	}

	return USB_INIT_DEVICE;
}
#endif

static int setup_fec(void)
{
	return set_clk_enet(ENET_125MHZ);
}

int board_phy_config(struct phy_device *phydev)
{
	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}

static int setup_eqos(void)
{
	struct blk_ctrl_wakeupmix_regs *bctrl =
		(struct blk_ctrl_wakeupmix_regs *)BLK_CTRL_WAKEUPMIX_BASE_ADDR;

	/* set INTF as RGMII, enable RGMII TXC clock */
	clrsetbits_le32(&bctrl->eqos_gpr,
			BCTRL_GPR_ENET_QOS_INTF_MODE_MASK,
			BCTRL_GPR_ENET_QOS_INTF_SEL_RGMII | BCTRL_GPR_ENET_QOS_CLK_GEN_EN);

	return set_clk_eqos(ENET_125MHZ);
}

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

int board_init(void)
{
#ifdef CONFIG_USB_TCPC
	setup_typec();
#endif

	/* SOM init */
	ccimx9_init();

	if (IS_ENABLED(CONFIG_FEC_MXC))
		setup_fec();

	if (IS_ENABLED(CONFIG_DWC_ETH_QOS))
		setup_eqos();

	if (IS_ENABLED(CONFIG_RTC_RV3028))
		setup_rv3028();

	return 0;
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, struct bd_info *bd)
{
	fdt_fixup_ccimx9(blob);
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
	env_set("sec_boot", "no");
#ifdef CONFIG_AHAB_BOOT
	env_set("sec_boot", "yes");
#endif

	/* SOM late init */
	ccimx9_late_init();

	/* Set default dynamic variables */
	platform_default_environment();

	return 0;
}

int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
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
	return 0;
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
