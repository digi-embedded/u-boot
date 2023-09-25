/*
 * Copyright (C) 2012-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2013-2018 Digi International, Inc.
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
#include <command.h>
#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/iomux.h>
#include <asm/arch/mx6-ddr.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <linux/errno.h>
#include <linux/mtd/mtd.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/mach-imx/hab.h>
#include <i2c.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
#include <mmc.h>
#include <fsl_esdhc_imx.h>
#include <otf_update.h>
#include <part.h>
#include <recovery.h>
#ifdef CONFIG_OF_LIBFDT
#include <fdt_support.h>
#endif
#include "../common/carrier_board.h"
#include "../common/helper.h"
#include "../common/hwid.h"
#include "../common/trustfence.h"
#include "ccimx6.h"
#include "../../../drivers/net/fec_mxc.h"

DECLARE_GLOBAL_DATA_PTR;

extern unsigned int board_version;
extern unsigned int board_id;
extern void board_spurious_wakeup(void);
#ifdef CONFIG_HAS_TRUSTFENCE
extern int rng_swtest_status;
#endif

static struct digi_hwid my_hwid;
static int enet_xcv_type;

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |               \
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define USDHC_PAD_CTRL (PAD_CTL_PKE | PAD_CTL_PUE |            \
	PAD_CTL_PUS_47K_UP  | PAD_CTL_SPEED_LOW |               \
	PAD_CTL_DSE_80ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

#define ENET_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED   |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_HYS)

#define SPI_PAD_CTRL (PAD_CTL_HYS |				\
	PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm | PAD_CTL_SRE_FAST)

#define I2C_PAD_CTRL	(PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm | PAD_CTL_HYS |			\
	PAD_CTL_ODE | PAD_CTL_SRE_FAST)

#define PC MUX_PAD_CTRL(I2C_PAD_CTRL)
/* I2C2 Camera, MIPI, pfuze */
static struct i2c_pads_info i2c_pad_info1 = {
	.scl = {
		.i2c_mode = MX6_PAD_KEY_COL3__I2C2_SCL | PC,
		.gpio_mode = MX6_PAD_KEY_COL3__GPIO4_IO12 | PC,
		.gp = IMX_GPIO_NR(4, 12)
	},
	.sda = {
		.i2c_mode = MX6_PAD_KEY_ROW3__I2C2_SDA | PC,
		.gpio_mode = MX6_PAD_KEY_ROW3__GPIO4_IO13 | PC,
		.gp = IMX_GPIO_NR(4, 13)
	}
};
#ifdef CONFIG_I2C_MULTI_BUS
static struct i2c_pads_info i2c_pad_info2 = {
	.scl = {
		.i2c_mode = MX6_PAD_GPIO_3__I2C3_SCL | PC,
		.gpio_mode = MX6_PAD_GPIO_3__GPIO1_IO03| PC,
		.gp = IMX_GPIO_NR(1, 3)
	},
	.sda = {
		.i2c_mode = MX6_PAD_GPIO_6__I2C3_SDA | PC,
		.gpio_mode = MX6_PAD_GPIO_6__GPIO1_IO06 | PC,
		.gp = IMX_GPIO_NR(1, 6)
	}
};
#endif

struct addrvalue {
	u32 address;
	u32 value;
};

/**
 * To add new valid variant ID, append new lines in this array with its configuration
 */
static struct ccimx6_variant ccimx6_variants[] = {
/* 0x00 */ { IMX6_NONE,	0, 0, "Unknown"},
/* 0x01 - 55001818-01 */
	{
		IMX6Q,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Consumer quad-core 1.2GHz, 4GB eMMC, 1GB DDR3, 0/+70C, Wireless, Bluetooth, Kinetis",
	},
/* 0x02 - 55001818-02 */
	{
		IMX6Q,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Consumer quad-core 1.2GHz, 4GB eMMC, 1GB DDR3, -20/+70C, Wireless, Bluetooth, Kinetis",
	},
/* 0x03 - 55001818-03 */
	{
		IMX6Q,
		SZ_512M,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH | CCIMX6_HAS_EMMC,
		"Industrial quad-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C, Wireless, Bluetooth",
	},
/* 0x04 - 55001818-04 */
	{
		IMX6D,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH | CCIMX6_HAS_EMMC,
		"Industrial dual-core 800MHz, 4GB eMMC, 1GB DDR3, -40/+85C, Wireless, Bluetooth",
	},
/* 0x05 - 55001818-05 */
	{
		IMX6D,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_EMMC,
		"Consumer dual-core 1GHz, 4GB eMMC, 1GB DDR3, 0/+70C, Wireless",
	},
/* 0x06 - 55001818-06 */
	{
		IMX6D,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH | CCIMX6_HAS_EMMC,
		"Consumer dual-core 1GHz, 4GB eMMC, 512MB DDR3, 0/+70C, Wireless, Bluetooth",
	},
/* 0x07 - 55001818-07 */
	{
		IMX6S,
		SZ_256M,
		CCIMX6_HAS_WIRELESS,
		"Consumer mono-core 1GHz, no eMMC, 256MB DDR3, 0/+70C, Wireless",
	},
/* 0x08 - 55001818-08 */
	{
		IMX6D,
		SZ_512M,
		CCIMX6_HAS_EMMC,
		"Consumer dual-core 1GHz, 4GB eMMC, 512MB DDR3, 0/+70C",
	},
/* 0x09 - 55001818-09 */
	{
		IMX6S,
		SZ_256M,
		0,
		"Consumer mono-core 1GHz, no eMMC, 256MB DDR3, 0/+70C",
	},
/* 0x0A - 55001818-10 */
	{
		IMX6DL,
		SZ_512M,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_EMMC,
		"Industrial DualLite-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C, Wireless",
	},
/* 0x0B - 55001818-11 */
	{
		IMX6DL,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH | CCIMX6_HAS_EMMC,
		"Consumer DualLite-core 1GHz, 4GB eMMC, 1GB DDR3, 0/+70C, Wireless, Bluetooth",
	},
/* 0x0C - 55001818-12 */
	{
		IMX6DL,
		SZ_512M,
		CCIMX6_HAS_EMMC,
		"Industrial DualLite-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C",
	},
/* 0x0D - 55001818-13 */
	{
		IMX6D,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Industrial dual-core 800MHz, 8GB eMMC, 1GB DDR3, -40/+85C, Wireless, Bluetooth, Kinetis",
	},
/* 0x0E - 55001818-14 */
	{
		IMX6D,
		SZ_512M,
		CCIMX6_HAS_EMMC,
		"Industrial dual-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C",
	},
/* 0x0F - 55001818-15 */
	{
		IMX6Q,
		SZ_512M,
		CCIMX6_HAS_EMMC,
		"Industrial quad-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C",
	},
/* 0x10 - 55001818-16 */
	{
		IMX6Q,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Industrial quad-core 800MHz, 4GB eMMC, 1GB DDR3, -40/+85C, Wireless, Bluetooth, Kinetis",
	},
/* 0x11 - 55001818-17 */
	{
		IMX6Q,
		SZ_1G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Industrial quad-core 800MHz, 8GB eMMC, 1GB DDR3, -40/+85C, Wireless, Bluetooth, Kinetis",
	},
/* 0x12 - 55001818-18 */
	{
		IMX6Q,
		SZ_2G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_EMMC,
		"Consumer quad-core 1.2GHz, 4GB eMMC, 2GB DDR3, -20/+70C, Wireless, Bluetooth",
	},
/* 0x13 - 55001818-19 */
	{
		IMX6DL,
		SZ_512M,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_EMMC,
		"Industrial DualLite-core 800MHz, 4GB eMMC, 512MB DDR3, -40/+85C, Wireless, Bluetooth",
	},
/* 0x14 - 55001818-20 */
	{
		IMX6D,
		SZ_1G,
		CCIMX6_HAS_EMMC,
		"Consumer dual-core 1GHz, 4GB eMMC, 1GB DDR3, 0/+70C",
	},
/* 0x15 - 55001818-21 */
	{
		IMX6DL,
		SZ_1G,
		CCIMX6_HAS_EMMC,
		"Industrial DualLite-core 800MHz, 4GB eMMC, 1GB DDR3, -40/+85C",
	},
};

#define NUM_VARIANTS_CC6	21

#define DDR3_CAL_REGS	12
/* DDR3 calibration values for the different CC6 variants */
static struct addrvalue ddr3_cal_cc6[NUM_VARIANTS_CC6 + 1][DDR3_CAL_REGS] = {
	/* Variant 0x02 */
	[0x02] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00070012},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002C0020},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x001F0035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x002E0030},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x432C0331},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03250328},
		{MX6_MMDC_P1_MPDGCTRL0, 0x433E0346},
		{MX6_MMDC_P1_MPDGCTRL1, 0x0336031C},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x382B2F35},
		{MX6_MMDC_P1_MPRDDLCTL, 0x31332A3B},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3938403A},
		{MX6_MMDC_P1_MPWRDLCTL, 0x4430453D},
	},
	/* Variant 0x03 (same as variant 0x0F) */
	[0x03] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000C0019},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00310024},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43450348},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03330339},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3F38393C},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3A3B433F},
		{0, 0},
	},
	/* Variant 0x04 (same as variant 0x0D) */
	[0x04] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000B0018},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00320023},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00200038},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300033},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43430345},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03370339},
		{MX6_MMDC_P1_MPDGCTRL0, 0x43500356},
		{MX6_MMDC_P1_MPDGCTRL1, 0x0348032A},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3E33353A},
		{MX6_MMDC_P1_MPRDDLCTL, 0x37383141},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3B3A433D},
		{MX6_MMDC_P1_MPWRDLCTL, 0x4633483E},
	},
	/* Variant 0x05 (same as variant 0x14) */
	[0x05] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00080014},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00300022},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00200035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300032},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x432F0332},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03250328},
		{MX6_MMDC_P1_MPDGCTRL0, 0x433D0345},
		{MX6_MMDC_P1_MPDGCTRL1, 0x0339031C},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3B303438},
		{MX6_MMDC_P1_MPRDDLCTL, 0x32342D3C},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3938433C},
		{MX6_MMDC_P1_MPWRDLCTL, 0x4433463D},
	},
	/* Variant 0x06 (same as 0x08) */
	[0x06] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000A0015},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002E0020},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43360337},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0329032B},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x39303338},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x37373F3A},
		{0, 0},
	},
	/* Variant 0x07 (same as 0x09) */
	[0x07] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00290036},
		{0, 0},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x42540247},
		{0, 0},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x40404847},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x40402D31},
		{0, 0},
	},
	/* Variant 0x08 (same as 0x06) */
	[0x08] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000A0015},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002E0020},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43360337},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0329032B},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x39303338},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x37373F3A},
		{0, 0},
	},
	/* Variant 0x09 (same as 0x07) */
	[0x09] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00290036},
		{0, 0},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x42540247},
		{0, 0},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x40404847},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x40402D31},
		{0, 0},
	},
	/* Variant 0x0A (same as variants 0x0C, 0x13) */
	[0x0A] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00270036},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00310033},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x42520243},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0236023F},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x45474B4A},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x28282326},
		{0, 0},
	},
	/* Variant 0x0B */
	[0x0B] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x002C0038},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00360038},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x001B001F},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x002B0034},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x423F0235},
		{MX6_MMDC_P0_MPDGCTRL1, 0x02360241},
		{MX6_MMDC_P1_MPDGCTRL0, 0x42340236},
		{MX6_MMDC_P1_MPDGCTRL1, 0x02250238},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x41454848},
		{MX6_MMDC_P1_MPRDDLCTL, 0x45464B43},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x36352D31},
		{MX6_MMDC_P1_MPWRDLCTL, 0x3130332D},
	},
	/* Variant 0x0C (same as variants 0x0A, 0x13) */
	[0x0C] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00270036},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00310033},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x42520243},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0236023F},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x45474B4A},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x28282326},
		{0, 0},
	},
	/* Variant 0x0D (same as variant 0x04) */
	[0x0D] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000B0018},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00320023},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00200038},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300033},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43430345},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03370339},
		{MX6_MMDC_P1_MPDGCTRL0, 0x43500356},
		{MX6_MMDC_P1_MPDGCTRL1, 0x0348032A},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3E33353A},
		{MX6_MMDC_P1_MPRDDLCTL, 0x37383141},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3B3A433D},
		{MX6_MMDC_P1_MPWRDLCTL, 0x4633483E},
	},
	/* Variant 0x0E */
	[0x0E] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x0011001B},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00370029},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x4348034A},
		{MX6_MMDC_P0_MPDGCTRL1, 0x033C033E},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3F36383E},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3D3C4440},
		{0, 0},
	},
	/* Variant 0x0F (same as variant 0x03) */
	[0x0F] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x000C0019},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00310024},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43450348},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03330339},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3F38393C},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3A3B433F},
		{0, 0},
	},
	/* Variant 0x11 */
	[0x11] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x0013001E},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x003B002D},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00280041},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x0037003E},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x434C034F},
		{MX6_MMDC_P0_MPDGCTRL1, 0x033F0344},
		{MX6_MMDC_P1_MPDGCTRL0, 0x4358035F},
		{MX6_MMDC_P1_MPDGCTRL1, 0x034E0332},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3D33353A},
		{MX6_MMDC_P1_MPRDDLCTL, 0x37383240},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3C3B453E},
		{MX6_MMDC_P1_MPWRDLCTL, 0x47374A40},
	},
	/* Variant 0x12 */
	[0x12] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00060015},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002F001F},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00220035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300031},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43220325},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0318031F},
		{MX6_MMDC_P1_MPDGCTRL0, 0x4334033C},
		{MX6_MMDC_P1_MPDGCTRL1, 0x032F0314},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3E31343B},
		{MX6_MMDC_P1_MPRDDLCTL, 0x38363040},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3939423B},
		{MX6_MMDC_P1_MPWRDLCTL, 0x46354840},
	},
	/* Variant 0x13 (same as variants 0x0A, 0x0C) */
	[0x13] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00270036},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00310033},
		{0, 0},
		{0, 0},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x42520243},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0236023F},
		{0, 0},
		{0, 0},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x45474B4A},
		{0, 0},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x28282326},
		{0, 0},
	},
	/* Variant 0x14 (same as variant 0x05) */
	[0x14] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00080014},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00300022},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00200035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300032},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x432F0332},
		{MX6_MMDC_P0_MPDGCTRL1, 0x03250328},
		{MX6_MMDC_P1_MPDGCTRL0, 0x433D0345},
		{MX6_MMDC_P1_MPDGCTRL1, 0x0339031C},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3B303438},
		{MX6_MMDC_P1_MPRDDLCTL, 0x32342D3C},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3938433C},
		{MX6_MMDC_P1_MPWRDLCTL, 0x4433463D},
	},
	/* Variant 0x15 (similar to variant 0x0B). Calibration pending) */
	[0x15] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x002C0038},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x00360038},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x001B001F},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x002B0034},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x423F0235},
		{MX6_MMDC_P0_MPDGCTRL1, 0x02360241},
		{MX6_MMDC_P1_MPDGCTRL0, 0x42340236},
		{MX6_MMDC_P1_MPDGCTRL1, 0x02250238},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x41454848},
		{MX6_MMDC_P1_MPRDDLCTL, 0x45464B43},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x36352D31},
		{MX6_MMDC_P1_MPWRDLCTL, 0x3130332D},
	},
};

/**
 * To add new valid variant ID, append new lines in this array with its configuration
 */
static struct ccimx6_variant ccimx6p_variants[] = {
/* 0x00 */ { IMX6_NONE,	0, 0, "Unknown"},
/* 0x01 - 55001983-01 */
	{
		IMX6QP,
		SZ_2G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Industrial QuadPlus-core 1GHz, 8GB eMMC, 2GB DDR3, -40/+85C, Wireless, Bluetooth, Kinetis",
	},
/* 0x02 - 55001983-02 */
	{
		IMX6QP,
		SZ_2G,
		CCIMX6_HAS_WIRELESS | CCIMX6_HAS_BLUETOOTH |
		CCIMX6_HAS_KINETIS | CCIMX6_HAS_EMMC,
		"Automotive QuadPlus-core 1GHz, 8GB eMMC, 2GB DDR3, -40/+85C, Wireless, Bluetooth, Kinetis",
	},
/* 0x03 - 55001983-03 */
	{
		IMX6DP,
		SZ_1G,
		CCIMX6_HAS_EMMC,
		"Industrial DualPlus-core 800MHz, 4GB eMMC, 1GB DDR3, -40/+85C",
	},
};
#define NUM_VARIANTS_CC6P	4

/* DDR3 calibration values for the different CC6+ variants */
static struct addrvalue ddr3_cal_cc6p[NUM_VARIANTS_CC6P + 1][DDR3_CAL_REGS] = {
	/* Variant 0x01 */
	[0x01] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00060015},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002F001F},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00220035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300031},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43220325},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0318031F},
		{MX6_MMDC_P1_MPDGCTRL0, 0x4334033C},
		{MX6_MMDC_P1_MPDGCTRL1, 0x032F0314},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3E31343B},
		{MX6_MMDC_P1_MPRDDLCTL, 0x38363040},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3939423B},
		{MX6_MMDC_P1_MPWRDLCTL, 0x46354840},
	},

	/* Variant 0x02 (same as variant 0x01) */
	[0x02] = {
		/* Write leveling */
		{MX6_MMDC_P0_MPWLDECTRL0, 0x00060015},
		{MX6_MMDC_P0_MPWLDECTRL1, 0x002F001F},
		{MX6_MMDC_P1_MPWLDECTRL0, 0x00220035},
		{MX6_MMDC_P1_MPWLDECTRL1, 0x00300031},
		/* Read DQS gating */
		{MX6_MMDC_P0_MPDGCTRL0, 0x43220325},
		{MX6_MMDC_P0_MPDGCTRL1, 0x0318031F},
		{MX6_MMDC_P1_MPDGCTRL0, 0x4334033C},
		{MX6_MMDC_P1_MPDGCTRL1, 0x032F0314},
		/* Read delay */
		{MX6_MMDC_P0_MPRDDLCTL, 0x3E31343B},
		{MX6_MMDC_P1_MPRDDLCTL, 0x38363040},
		/* Write delay */
		{MX6_MMDC_P0_MPWRDLCTL, 0x3939423B},
		{MX6_MMDC_P1_MPWRDLCTL, 0x46354840},
	},
        /* Variant 0x03 (copied from CC6 var 0x02 pending calibration) */
        [0x03] = {
                /* Write leveling */
                {MX6_MMDC_P0_MPWLDECTRL0, 0x00070012},
                {MX6_MMDC_P0_MPWLDECTRL1, 0x002C0020},
                {MX6_MMDC_P1_MPWLDECTRL0, 0x001F0035},
                {MX6_MMDC_P1_MPWLDECTRL1, 0x002E0030},
                /* Read DQS gating */
                {MX6_MMDC_P0_MPDGCTRL0, 0x432C0331},
                {MX6_MMDC_P0_MPDGCTRL1, 0x03250328},
                {MX6_MMDC_P1_MPDGCTRL0, 0x433E0346},
                {MX6_MMDC_P1_MPDGCTRL1, 0x0336031C},
                /* Read delay */
                {MX6_MMDC_P0_MPRDDLCTL, 0x382B2F35},
                {MX6_MMDC_P1_MPRDDLCTL, 0x31332A3B},
                /* Write delay */
                {MX6_MMDC_P0_MPWRDLCTL, 0x3938403A},
                {MX6_MMDC_P1_MPWRDLCTL, 0x4430453D},
        },
};

static struct ccimx6_variant * get_cc6_variant(u8 variant)
{
	if (is_mx6dqp()) {
		if (variant > ARRAY_SIZE(ccimx6p_variants))
			return NULL;
		return &ccimx6p_variants[variant];
	} else {
		if (variant > ARRAY_SIZE(ccimx6_variants))
			return NULL;
		return &ccimx6_variants[variant];
	}
}

static void update_ddr3_calibration(u8 variant)
{
	int i;
	volatile u32 *addr;
	struct addrvalue *ddr3_cal;

	if (is_mx6dqp()) {
		if (variant == 0 || variant > ARRAY_SIZE(ddr3_cal_cc6p))
			return;
		ddr3_cal = ddr3_cal_cc6p[variant];
	} else {
		if (variant == 0 || variant > ARRAY_SIZE(ddr3_cal_cc6))
			return;
		ddr3_cal = ddr3_cal_cc6[variant];
	}

	for (i = 0; i < DDR3_CAL_REGS; i++) {
		addr = (volatile u32 *)(ddr3_cal[i].address);
		if (addr != NULL)
			writel(ddr3_cal[i].value, addr);
	}
}

int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}

static iomux_v3_cfg_t const enet_pads_100[] = {
	MX6_PAD_ENET_MDIO__ENET_MDIO		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_MDC__ENET_MDC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TXD0__ENET_TX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TXD1__ENET_TX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RXD0__ENET_RX_DATA0	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RXD1__ENET_RX_DATA1	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TX_CTL__ENET_REF_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_RX_ER__ENET_RX_ER		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_TX_EN__ENET_TX_EN		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_CRS_DV__ENET_RX_EN		| MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static iomux_v3_cfg_t const enet_pads_1000[] = {
	MX6_PAD_ENET_MDIO__ENET_MDIO		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_MDC__ENET_MDC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TXC__RGMII_TXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD0__RGMII_TD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD1__RGMII_TD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD2__RGMII_TD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TD3__RGMII_TD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_TX_CTL__RGMII_TX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RXC__RGMII_RXC		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD0__RGMII_RD0		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD1__RGMII_RD1		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD2__RGMII_RD2		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RD3__RGMII_RD3		| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_RGMII_RX_CTL__RGMII_RX_CTL	| MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET_REF_CLK__ENET_TX_CLK	| MUX_PAD_CTRL(ENET_PAD_CTRL),
};

void setup_iomux_enet(void)
{
	int enet;

	/* iomux for Gigabit or 10/100 and PHY selection
	 * basing on env variable 'ENET'. Default to Gigabit.
	 */
	enet = (int)env_get_ulong("ENET", 10, 1000);
	if (enet == 100) {
		/* 10/100 ENET */
		enet_xcv_type = RMII;
		imx_iomux_v3_setup_multiple_pads(enet_pads_100,
						 ARRAY_SIZE(enet_pads_100));
	} else {
		/* Gigabit ENET */
		enet_xcv_type = RGMII;
		imx_iomux_v3_setup_multiple_pads(enet_pads_1000,
						 ARRAY_SIZE(enet_pads_1000));
	}
}

int board_get_enet_xcv_type(void)
{
	return enet_xcv_type;
}

static int pmic_access_page(unsigned char page)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_PMIC_I2C_BUS))
		return -1;
#endif

	if (i2c_probe(CONFIG_PMIC_I2C_ADDR)) {
		printf("ERR: cannot access the PMIC\n");
		return -1;
	}

	if (i2c_write(CONFIG_PMIC_I2C_ADDR, DA9063_PAGE_CON, 1, &page, 1)) {
		printf("Cannot set PMIC page!\n");
		return -1;
	}

	return 0;
}

int pmic_read_reg(int reg, unsigned char *value)
{
	unsigned char page = reg / 0x80;

	if (pmic_access_page(page))
		return -1;

	if (i2c_read(CONFIG_PMIC_I2C_ADDR, reg, 1, value, 1))
		return -1;

	/* return to page 0 by default */
	pmic_access_page(0);
	return 0;
}

int pmic_write_reg(int reg, unsigned char value)
{
	unsigned char page = reg / 0x80;

	if (pmic_access_page(page))
		return -1;

	if (i2c_write(CONFIG_PMIC_I2C_ADDR, reg, 1, &value, 1))
		return -1;

	/* return to page 0 by default */
	pmic_access_page(0);
	return 0;
}

int pmic_write_bitfield(int reg, unsigned char mask, unsigned char off,
			       unsigned char bfval)
{
	unsigned char value;

	if (pmic_read_reg(reg, &value) == 0) {
		value &= ~(mask << off);
		value |= (bfval << off);
		return pmic_write_reg(reg, value);
	}

	return -1;
}

static bool is_valid_hwid(struct digi_hwid *hwid)
{
	int num;
	struct ccimx6_variant *cc6_variant = get_cc6_variant(hwid->variant);

	num = is_mx6dqp() ? ARRAY_SIZE(ccimx6p_variants) :
			    ARRAY_SIZE(ccimx6_variants);

	if (hwid->variant < num && cc6_variant != NULL)
		if (cc6_variant->cpu != IMX6_NONE)
			return 1;

	return 0;
}

bool board_has_emmc(void)
{
	struct ccimx6_variant *cc6_variant = get_cc6_variant(my_hwid.variant);

	if (is_valid_hwid(&my_hwid))
		return !!(cc6_variant->capabilities & CCIMX6_HAS_EMMC);
	else
		return true; /* assume it has if invalid HWID */
}

static bool board_has_wireless(void)
{
	struct ccimx6_variant *cc6_variant = get_cc6_variant(my_hwid.variant);

	if (is_valid_hwid(&my_hwid))
		return !!(cc6_variant->capabilities & CCIMX6_HAS_WIRELESS);
	else
		return true; /* assume it has if invalid HWID */
}

static bool board_has_bluetooth(void)
{
	struct ccimx6_variant *cc6_variant = get_cc6_variant(my_hwid.variant);

	if (is_valid_hwid(&my_hwid))
		return !!(cc6_variant->capabilities & CCIMX6_HAS_BLUETOOTH);
	else
		return true; /* assume it has if invalid HWID */
}

static bool board_has_kinetis(void)
{
	struct ccimx6_variant *cc6_variant = get_cc6_variant(my_hwid.variant);

	if (is_valid_hwid(&my_hwid))
		return !!(cc6_variant->capabilities & CCIMX6_HAS_KINETIS);
	else
		return true; /* assume it has if invalid HWID */
}

#ifdef CONFIG_FSL_ESDHC_IMX

/* The order of MMC controllers here must match that of CONFIG_MMCDEV_USDHCx
 * in the platform header
 */
static struct fsl_esdhc_cfg usdhc_cfg[CFG_SYS_FSL_USDHC_NUM] = {
	{USDHC4_BASE_ADDR},
	{USDHC2_BASE_ADDR},
};

static iomux_v3_cfg_t const usdhc4_pads[] = {
	MX6_PAD_SD4_CLK__SD4_CLK	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_CMD__SD4_CMD	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT0__SD4_DATA0	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT1__SD4_DATA1	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT2__SD4_DATA2	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT3__SD4_DATA3	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT4__SD4_DATA4	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT5__SD4_DATA5	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT6__SD4_DATA6	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD4_DAT7__SD4_DATA7	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

static iomux_v3_cfg_t const usdhc2_pads[] = {
	MX6_PAD_SD2_CLK__SD2_CLK	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_CMD__SD2_CMD	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT0__SD2_DATA0	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT1__SD2_DATA1	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT2__SD2_DATA2	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
	MX6_PAD_SD2_DAT3__SD2_DATA3	| MUX_PAD_CTRL(USDHC_PAD_CTRL),
};

int mmc_get_bootdevindex(void)
{
	switch(get_boot_device()) {
	case SD1_BOOT ... SD4_BOOT:
		/* SD card */
		if (board_has_emmc())
			return 1;	/* index of USDHC2 if SOM has eMMC */
		else
			return 0;	/* index of USDHC2 if SOM has no eMMC */
	case MMC4_BOOT:
		return 0;	/* index of SDHC4 (eMMC) */
	default:
		/* return default value otherwise */
		return EMMC_BOOT_DEV;
	}
}

int board_mmc_get_env_dev(int devno)
{
	return mmc_get_bootdevindex();
}

int board_mmc_get_env_part(int devno)
{
	switch(get_boot_device()) {
	case SD1_BOOT ... SD4_BOOT:
		return 0;	/* When booting from an SD card the
				 * environment will be saved to the unique
				 * hardware partition: 0 */
	case MMC4_BOOT:
	default:
		return CONFIG_SYS_MMC_ENV_PART;
				/* When booting from SDHC4 (eMMC) the
				 * environment will be saved to boot
				 * partition 2 to protect it from
				 * accidental overwrite during U-Boot update */
	}
}

int board_mmc_init(struct bd_info *bis)
{
	int ret;
	int i;

	for (i = 0; i < CFG_SYS_FSL_USDHC_NUM; i++) {
		switch (i) {
		case 0:
			if (board_has_emmc()) {
				/* USDHC4 (eMMC) */
				imx_iomux_v3_setup_multiple_pads(usdhc4_pads,
						ARRAY_SIZE(usdhc4_pads));
				usdhc_cfg[i].sdhc_clk = mxc_get_clock(MXC_ESDHC4_CLK);
				ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
				if (ret)
					return ret;
			}
			break;
		case 1:
			/* USDHC2 (uSD) */

			/*
			 * On CC6PLUS enable LDO9 regulator powering USDHC2
			 * (microSD)
			 */
			if (is_mx6dqp())
				pmic_write_bitfield(DA9063_LDO9_CONT, 0x1, 0,
						    0x1);

			imx_iomux_v3_setup_multiple_pads(
					usdhc2_pads, ARRAY_SIZE(usdhc2_pads));
			usdhc_cfg[i].sdhc_clk = mxc_get_clock(MXC_ESDHC2_CLK);
			ret = fsl_esdhc_initialize(bis, &usdhc_cfg[i]);
			if (ret)
				return ret;
			break;
		default:
			printf("Warning: you configured more USDHC controllers"
				"(%d) than supported by the board\n", i + 1);
			return -EINVAL;
		}

	}

	return 0;
}
#endif /* CONFIG_FSL_ESDHC_IMX */

#ifdef CONFIG_SATA
int setup_iomux_sata(void)
{
	struct iomuxc *const iomuxc_regs = (struct iomuxc *) IOMUXC_BASE_ADDR;
	int ret = enable_sata_clock();
	if (ret)
		return ret;

	clrsetbits_le32(&iomuxc_regs->gpr[13],
			IOMUXC_GPR13_SATA_MASK,
			IOMUXC_GPR13_SATA_PHY_8_RXEQ_3P0DB
			|IOMUXC_GPR13_SATA_PHY_7_SATA2M
			|IOMUXC_GPR13_SATA_SPEED_3G
			|(3<<IOMUXC_GPR13_SATA_PHY_6_SHIFT)
			|IOMUXC_GPR13_SATA_SATA_PHY_5_SS_DISABLED
			|IOMUXC_GPR13_SATA_SATA_PHY_4_ATTEN_9_16
			|IOMUXC_GPR13_SATA_PHY_3_TXBOOST_0P00_DB
			|IOMUXC_GPR13_SATA_PHY_2_TX_1P104V
			|IOMUXC_GPR13_SATA_PHY_1_SLOW);

	return 0;
}
#endif /* CONFIG_SATA */

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
	{"sd2",	 MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"sd3",	 MAKE_CFGVAL(0x40, 0x30, 0x00, 0x00)},
	/* 8 bit bus width */
	{"emmc", MAKE_CFGVAL(0x60, 0x58, 0x00, 0x00)},
	{NULL,	 0},
};
#endif

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
		/* increase VDDARM to 1.450V */
		if (pmic_read_reg(DA9063_VBCORE1_A_ADDR, &value)) {
			printf("Read BCORE1 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x73;
		if (pmic_write_reg(DA9063_VBCORE1_A_ADDR, value)) {
			printf("Set BCORE1 error!\n");
			goto out;
		}

		/* increase VDDSOC to 1.450V */
		if (pmic_read_reg(DA9063_VBCORE2_A_ADDR, &value)) {
			printf("Read BCORE2 error!\n");
			goto out;
		}
		value &= ~0x7f;
		value |= 0x73;
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

static int print_pmic_info(void)
{
	unsigned char dev_id, var_id, conf_id, cust_id;

	/* Read and print PMIC identification */
	if (pmic_read_reg(DA9063_DEVICE_ID_ADDR, &dev_id) ||
	    pmic_read_reg(DA9063_VARIANT_ID_ADDR, &var_id) ||
	    pmic_read_reg(DA9063_CUSTOMER_ID_ADDR, &cust_id) ||
	    pmic_read_reg(DA9063_CONFIG_ID_ADDR, &conf_id)) {
		printf("Could not read PMIC ID registers\n");
		return -1;
	}
	printf("PMIC:  DA9063, Device: 0x%02x, Variant: 0x%02x, "
		"Customer: 0x%02x, Config: 0x%02x\n", dev_id, var_id,
		cust_id, conf_id);

	return 0;
}

static void ccimx6_detect_spurious_wakeup(void)
{
	unsigned char event_a, event_b, event_c, event_d, fault_log;

	/* Check whether we come from a shutdown state */
	pmic_read_reg(DA9063_FAULT_LOG_ADDR, &fault_log);
	debug("DA9063 fault_log 0x%08x\n", fault_log);

	if (fault_log & (DA9063_E_nSHUT_DOWN | DA9063_E_nKEY_RESET)) {
		/* Clear fault log nSHUTDOWN or nKEY_RESET bit */
		pmic_write_reg(DA9063_FAULT_LOG_ADDR, fault_log &
			      (DA9063_E_nSHUT_DOWN | DA9063_E_nKEY_RESET));

		pmic_read_reg(DA9063_EVENT_A_ADDR, &event_a);
		pmic_read_reg(DA9063_EVENT_B_ADDR, &event_b);
		pmic_read_reg(DA9063_EVENT_C_ADDR, &event_c);
		pmic_read_reg(DA9063_EVENT_D_ADDR, &event_d);

		/* Clear event registers */
		pmic_write_reg(DA9063_EVENT_A_ADDR, event_a);
		pmic_write_reg(DA9063_EVENT_B_ADDR, event_b);
		pmic_write_reg(DA9063_EVENT_C_ADDR, event_c);
		pmic_write_reg(DA9063_EVENT_D_ADDR, event_d);

		/* Return if the wake up is valid */
		if (event_a) {
			/* Valid wake up sources include RTC ticks and alarm,
			 * onKey and ADC measurement */
			if (event_a & (DA9063_E_TICK | DA9063_E_ALARM |
				       DA9063_E_nONKEY | DA9063_E_ADC_RDY)) {
				debug("WAKE: Event A: 0x%02x\n", event_a);
				return;
			}
		}

		/* All events in B are wake-up capable  */
		if (event_b) {
			unsigned int valid_mask = 0xFF;

			/* Any event B is valid, except E_WAKE on SBCv1 which
			 * is N/C */
			if (((board_id == CCIMX6SBC_ID129) ||
			     (board_id == CCIMX6SBC_ID130) ||
			     (board_id == CCIMX6SBC_ID131)) &&
			    board_version <= 1)
				valid_mask &= ~DA9063_E_WAKE;

			if (event_b & valid_mask) {
				debug("WAKE: Event B: 0x%02x wake-up valid 0x%02x\n",
						event_b, valid_mask);
				return;
			}
		}

		/* The only wake-up OTP enabled GPIOs in event C are:
		 *   - GPIO5, valid on BT variants
		 *   - GPIO6, valid on wireless variants
		 */
		if (event_c) {
			unsigned int valid_mask = 0;

			/* On variants with bluetooth the BT_HOST_WAKE
			 * (GPIO5) pin is valid. */
			if (board_has_bluetooth())
				valid_mask |= DA9063_E_GPIO5;
			/* On variants with wireless the WLAN_HOST_WAKE
			 * (GPIO6) pin is valid. */
			if (board_has_wireless())
				valid_mask |= DA9063_E_GPIO6;

			if (event_c & valid_mask) {
				debug("WAKE: Event C: 0x%02x wake-up valid 0x%02x\n",
						event_c, valid_mask);
				return;
			}
		}

		/* The only wake-up OTP enabled GPIOs in event D are:
		 *  - GPIO8, valid on MCA variants
		 *  - GPIO9, N/C on SBCs, never valid
		 */
		if (event_d) {
			unsigned int valid_mask = 0;

			/* On variants with kinetis GPIO8/SYS_EN is valid */
			if (board_has_kinetis())
			       valid_mask |= DA9063_E_GPIO8;

			if (event_d & valid_mask) {
				debug("WAKE: Event D: 0x%02x wake-up valid 0x%02x\n",
						event_d, valid_mask);
				return;
			}
		}

		/* If we reach here the event is spurious */
		printf("Spurious wake, back to standby.\n");
		debug("Events A:%02x B:%02x C:%02x D:%02x\n", event_a, event_b,
			event_c, event_d);

		/* Make sure nRESET is asserted when waking up */
		pmic_write_bitfield(DA9063_CONTROL_B_ADDR, 0x1, 3, 0x1);

		/* Call board-specific actions for spurious wake situation */
		board_spurious_wakeup();

		/* De-assert SYS_EN to get to powerdown mode. The OTP
		 * is not reread when coming up so the wake-up
		 * supression configuration will be preserved .*/
		pmic_write_bitfield(DA9063_CONTROL_A_ADDR, 0x1, 0, 0x0);

		/* Don't come back */
		while (1)
			udelay(1000);
	}

	return;
}

static int ccimx6_fixup(void)
{
	if (!board_has_bluetooth()) {
		/* Avoid spurious wake ups */
		if (pmic_write_bitfield(DA9063_GPIO4_5_ADDR, 0x1, 7, 0x1)) {
			printf("Failed to suppress GPIO5 wakeup.");
			return -1;
		}
	}

	if (!board_has_wireless()) {
		/* Avoid spurious wake ups */
		if (pmic_write_bitfield(DA9063_GPIO6_7_ADDR, 0x1, 3, 0x1)) {
			printf("Failed to suppress GPIO6 wakeup.");
			return -1;
		}
	}

	if (!board_has_kinetis()) {
		/* Avoid spurious wake ups */
		if (pmic_write_bitfield(DA9063_GPIO8_9_ADDR, 0x1, 3, 0x1)) {
			printf("Failed to suppress GPIO8 wakeup.");
			return -1;
		}
	}

	ccimx6_detect_spurious_wakeup();

	return 0;
}

void pmic_bucks_synch_mode(void)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_PMIC_I2C_BUS))
                return;
#endif

	if (!i2c_probe(CONFIG_PMIC_I2C_ADDR)) {
		if (pmic_write_bitfield(DA9063_BCORE2_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BCORE2 in synchronous mode\n");
		if (pmic_write_bitfield(DA9063_BCORE1_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BCORE1 in synchronous mode\n");
		if (pmic_write_bitfield(DA9063_BPRO_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BPRO in synchronous mode\n");
		if (pmic_write_bitfield(DA9063_BIO_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BIO in synchronous mode\n");
		if (pmic_write_bitfield(DA9063_BMEM_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BMEM in synchronous mode\n");
		if (pmic_write_bitfield(DA9063_BPERI_CONF_ADDR, 0x3, 6, 0x2))
			printf("Could not set BPERI in synchronous mode\n");
	} else {
		printf("Could not set bucks in synchronous mode\n");
	}
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

	/* eMMC capacity is not exact, so asume 8GB if larger than 7GB */
	if (capacity_gb >= 7) {
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
	char var2[10];
	int i;

#ifdef CONFIG_CMD_MMC
	/* Set $mmcbootdev to MMC boot device index */
	sprintf(cmd, "setenv -f mmcbootdev %x", mmc_get_bootdevindex());
	run_command(cmd, 0);
#endif

	/* Build $soc_family variable */
	strcpy(var2, get_imx_family((get_cpu_rev() & 0xFF000) >> 12));
	/* Convert to lower case */
	for (i = 0; i < strlen(var2); i++)
		var2[i] = tolower(var2[i]);
	sprintf(var, "imx%s", var2);
	env_set("soc_family", var);

	/* Set $module_variant variable */
	sprintf(var, "0x%02x", my_hwid.variant);
	env_set("module_variant", var);

	/* Set $hwid_n variables */
	for (i = 0; i < CONFIG_HWID_WORDS_NUMBER; i++) {
		snprintf(var, sizeof(var), "hwid_%d", i);
		snprintf(var2, sizeof(var2), "%08x", ((u32 *) &my_hwid)[i]);
		env_set(var, var2);
	}

	/*
	 * If there are no defined partition tables generate them dynamically
	 * basing on the available eMMC size.
	 */
	generate_partition_table();
}

void board_update_hwid(bool is_fuse)
{
	/* Update HWID-related variables in environment */
	int ret = is_fuse ? board_sense_hwid(&my_hwid) : board_read_hwid(&my_hwid);

	if (ret)
		printf("Cannot read HWID\n");

	som_default_environment();
}

int ccimx6_late_init(void)
{
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_I2C_MULTI_BUS
	/* Setup I2C3 (HDMI, Audio...) */
	setup_i2c(2, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info2);
#endif
	if (print_pmic_info())
		return -1;

	/* Operate all PMIC's bucks in "synchronous" mode (PWM) since the
	 * default "auto" mode may change them to operate in "sleep" mode (PFD)
	 * which might result in malfunctioning on certain custom boards with
	 * low loads under extreme stress conditions.
	 */
	pmic_bucks_synch_mode();

	if (setup_pmic_voltages_carrierboard())
		return -1;

	/* Verify MAC addresses */
	verify_mac_address("ethaddr", DEFAULT_MAC_ETHADDR);

	if (board_has_wireless())
		verify_mac_address("wlanaddr", DEFAULT_MAC_WLANADDR);

	if (board_has_bluetooth())
		verify_mac_address("btaddr", DEFAULT_MAC_BTADDR);

#ifdef CONFIG_CONSOLE_ENABLE_PASSPHRASE
	gd->flags &= ~GD_FLG_DISABLE_CONSOLE_INPUT;
	if (!console_enable_passphrase())
		gd->flags &= ~(GD_FLG_DISABLE_CONSOLE | GD_FLG_SILENT);
	else
		gd->flags |= GD_FLG_DISABLE_CONSOLE_INPUT;
#endif

#ifdef CONFIG_HAS_TRUSTFENCE
	copy_dek();
#endif

	return ccimx6_fixup();
}

void print_ccimx6_info(void)
{
	struct ccimx6_variant *cc6_variant = get_cc6_variant(my_hwid.variant);

	if (is_valid_hwid(&my_hwid))
		printf("%s SOM variant 0x%02X: %s\n", CONFIG_SOM_DESCRIPTION,
		       my_hwid.variant, cc6_variant->id_string);
}

int ccimx6_init(void)
{
#ifdef CONFIG_HAS_TRUSTFENCE
	uint32_t ret;
	uint8_t event_data[36] = { 0 }; /* Event data buffer */
	size_t bytes = sizeof(event_data); /* Event size in bytes */
	enum hab_config config = 0;
	enum hab_state state = 0;
	hab_rvt_report_status_t *hab_report_status =
		(hab_rvt_report_status_t *)HAB_RVT_REPORT_STATUS;

	/* HAB event verification */
	ret = hab_report_status(&config, &state);
	if (ret == HAB_WARNING) {
		pr_debug("\nHAB Configuration: 0x%02x, HAB State: 0x%02x\n",
		       config, state);
		/* Verify RNG self test */
		rng_swtest_status = hab_event_warning_check(event_data, &bytes);
		if (rng_swtest_status == SW_RNG_TEST_PASSED) {
			printf("RNG:   self-test failed, but software test passed.\n");
		} else if (rng_swtest_status == SW_RNG_TEST_FAILED) {
#ifdef CONFIG_RNG_SELF_TEST
			printf("WARNING: RNG self-test and software test failed!\n");
#else
			printf("WARNING: RNG self-test failed!\n");
#endif
			if (imx_hab_is_enabled()) {
				printf("Aborting secure boot.\n");
				run_command("reset", 0);
			}
		}
	} else {
		rng_swtest_status = SW_RNG_TEST_NA;
	}
#endif /* CONFIG_HAS_TRUSTFENCE */

	if (board_read_hwid(&my_hwid)) {
		printf("Cannot read HWID\n");
		return -1;
	}

	/*
	 * Override DDR3 calibration values basing on HWID variant.
	 * NOTE: Re-writing the DDR3 calibration values is a delicate operation
	 * that must be done before other systems (like VPU) are enabled and can
	 * be accessing the RAM on their own.
	 */
	update_ddr3_calibration(my_hwid.variant);

	/* Setup I2C2 (PMIC, Kinetis) */
	setup_i2c(1, CONFIG_SYS_I2C_SPEED, 0x7f, &i2c_pad_info1);

	return 0;
}

void fdt_fixup_ccimx6(void *fdt)
{
	fdt_fixup_hwid(fdt, &my_hwid);

	if (board_has_wireless()) {
		/* Wireless MACs */
		fdt_fixup_mac(fdt, "wlanaddr", "/wireless", "mac-address");
		if (is_mx6dqp()) {
			fdt_fixup_mac(fdt, "wlan1addr", "/wireless", "mac-address1");
			fdt_fixup_mac(fdt, "wlan2addr", "/wireless", "mac-address2");
			fdt_fixup_mac(fdt, "wlan3addr", "/wireless", "mac-address3");
		}

		/* Regulatory domain */
		fdt_fixup_regulatory(fdt);
	}
	if (board_has_bluetooth())
		fdt_fixup_mac(fdt, "btaddr", "/bluetooth", "mac-address");
	fdt_fixup_trustfence(fdt);
	fdt_fixup_uboot_info(fdt);
}
