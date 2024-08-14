// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024, Digi International Inc.
 *
 * Author: Gonzalo Ruiz <gonzalo.ruiz@digi.com>
 *
 * This nhd-3.5-640480ef-msxp panel driver is inspired from the Linux Kernel driver
 * drivers/gpu/drm/panel/panel-forcelead-fl7703ni.c.
 */

#include <common.h>
#include <backlight.h>
#include <dm.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <dm/device_compat.h>
#include <linux/delay.h>
#include <power/regulator.h>

struct nhd35640480ef_panel_priv {
	struct udevice *vcc_reg;
	struct udevice *iovcc_reg;
	struct gpio_desc reset;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
};

static const struct display_timing default_timing = {
	.pixelclock.typ		= 31200000,
	.hactive.typ		= 640,
	.hfront_porch.typ	= 120,
	.hback_porch.typ	= 120,
	.hsync_len.typ		= 120,
	.vactive.typ		= 480,
	.vfront_porch.typ	= 25,
	.vback_porch.typ	= 12,
	.vsync_len.typ		= 5,
};

static void nhd35640480ef_dcs_write_buf(struct udevice *dev, const void *data,
				   size_t len)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;

	if (mipi_dsi_dcs_write_buffer(device, data, len) < 0)
		dev_err(dev, "mipi dsi dcs write buffer failed\n");
}

#define dcs_write_seq(dev, seq...)				\
({								\
	static const u8 d[] = { seq };				\
	nhd35640480ef_dcs_write_buf(dev, d, ARRAY_SIZE(d));		\
})

static int nhd35640480ef_read_id(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	u8 id1, id2, id3;
	int ret;

	ret = mipi_dsi_dcs_read(device, 0xDA, &id1, 1);
	if (ret < 0) {
		dev_err(dev, "could not read ID1\n");
		return ret;
	}

	ret = mipi_dsi_dcs_read(device, 0xDB, &id2, 1);
	if (ret < 0) {
		dev_err(dev, "could not read ID2\n");
		return ret;
	}

	ret = mipi_dsi_dcs_read(device, 0xDC, &id3, 1);
	if (ret < 0) {
		dev_err(dev, "could not read ID3\n");
		return ret;
	}

	dev_info(dev, "manufacturer: %02x version: %02x driver: %02x\n",
		     id1, id2, id3);

	return 0;
}

/* General initialization Packet (GIP) sequence */
static void nhd35640480ef_init_sequence(struct udevice *dev)
{
	dcs_write_seq(dev, MIPI_DCS_SOFT_RESET);

	/* We need to wait 5ms before sending new commands */
	mdelay(5);

	/* Enable USER command */
	dcs_write_seq(dev, 0xB9,  /* SETEXTC command */
			   0xF1, 0x12, 0x87);

	/* Set Display resolution */
	dcs_write_seq(dev, 0xB2,  /* SETDISP command */
			   0x78,  /* 480Gate(120*4+0) */
			   0x14,  /* 640RGB */
			   0x70);

	/* Set RGB */
	dcs_write_seq(dev, 0xB3,  /* SETRGBIF command */
			   0x10,  /* VBP_RGB_GEN */
			   0x10,  /* VFP_RGB_GEN */
			   0x28,  /* DE_BP_RGB_GEN */
			   0x28,  /* DE_FP_RGB_GEN */
			   0x03, 0xFF, 0x00, 0x00, 0x00, 0x00);

	/* Set Panel Inversion */
	dcs_write_seq(dev, 0xB4,  /* SETCYC command */
			   0x80); /* Zig-zag inversion type D */

	/* Set VREF/NVREF voltage */
	dcs_write_seq(dev, 0xB5,  /* SETBGP command */
			   0x0A,  /* VREF */
			   0x0A); /* NVREF */

	/* Set VCOM voltage */
	dcs_write_seq(dev, 0xB6,  /* SETVCOM command */
			   0x70,  /* F_VCOM */
			   0x70); /* B_VCOM */

	/* Set ECP */
	dcs_write_seq(dev, 0xB8,  /* SETPOWER_EXT command */
			   0x26);

	/* Set MIPI configurations */
	dcs_write_seq(dev, 0xBA,  /* SETMIPI command */
			   0x33, /* Set Lane_Number[1:0] to 4 Lanes */
			   0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00,
			   0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00,
			   0x91, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00,
			   0x00, 0x37);

	/* Set NVDDD/VDDD voltage */
	dcs_write_seq(dev, 0xBC,  /* SETVDC command */
			   0x47);

	/* Set PCR settings */
	dcs_write_seq(dev, 0xBF,
			   0x02, 0x11, 0x00);

	/* Set Source driving settings */
	dcs_write_seq(dev, 0xC0,  /* SETSCR command */
			   0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x73,
			   0x00);

	/* Set POWER settings */
	dcs_write_seq(dev, 0xC1,  /* SETPOWER command */
			   0x54,  /* VBTHS VBTLS */
			   0x00,
			   0x32,  /* VSPR */
			   0x32,  /* VSNR */
			   0x77,  /* VSP VSN */
			   0xF4,  /* APS */
			   0x77,  /* VGH1_L VGL1_L */
			   0x77,  /* VGH1_R VGL1_R */
			   0xCC,  /* VGH2_L VGL2_L */
			   0xCC,  /* VGH2_R VGL2_R */
			   0xFF,  /* VGH3_L VGL3_L */
			   0xFF,  /* VGH3_R VGL3_R */
			   0x11,  /* VGH4_L VGL4_L */
			   0x11,  /* VGH4_R VGL4_R */
			   0x00,  /* VGH5_L VGL5_L */
			   0x00,  /* VGH5_R VGL5_R */
			   0x31); /* VSP VSN DET EN */

	/* Set I/O settings */
	dcs_write_seq(dev, 0xC7,  /* SETIO command */
			   0x10,  /* Enable VOUT output*/
			   0x00, 0x0A, 0x00, 0x00,
			   0x00,  /* Set VOUT at 3.3V */
			   0x00,  /* D6-5:HOUT_SEL | D1-0:VOUT_SEL */
			   0x00,  /* D1-0:PWM_SEL */
			   0xED,  /* D7:MIPI_ERR_DIS_TE
				     D6:VGL_DET_DIS_TE
			             D5:VGH_SET_DIS_TE
				     D4:DBV_ZERO_DIS_TE
				     D3:LVPUR_DIS_TE
				     D2:TE_ONLY_AT_NORMAL
				     D1:CRC_MATC
				     D0:REF_CRC_DIS_TE */
			   0xC7,  /* D7:OLED_CMD
				     D6:N_CMD_EN
				     D5:HS_STOP_CMD_EN
				     D4:D6:MIPI_STOP_09_PAR3_EN
				     D3:CHECK_VSYNC
				     D2:MIPI_STOP_09_EN
				     D1:TE_STOP_RD_EN
				     D0:STATE_ERR_READ_EN */
			   0x00,
			   0xA5);

	/* Set CABC settings */
	dcs_write_seq(dev, 0xC8,  /* SETCABC command */
			   0x10, 0x40, 0x1E, 0x03);

	/* Set Panel settings */
	dcs_write_seq(dev, 0xCC,  /* SETPANEL command */
			   0x0B);

	/* Set Asymmetric gamma2.2 */
	dcs_write_seq(dev, 0xE0,  /* SETGAMMA command */
			   0x00, 0x05, 0x09, 0x29, 0x3C, 0x3F, 0x3B, 0x37,
			   0x05, 0x0A, 0x0C, 0x10, 0x13, 0x10, 0x13, 0x12,
			   0x1A, 0x00, 0x05, 0x09, 0x29, 0x3C, 0x3F, 0x3B,
			   0x37, 0x05, 0x0A, 0x0C, 0x10, 0x13, 0x10, 0x13,
			   0x12, 0x1A);

	dcs_write_seq(dev, 0xE1,
			   0x11, 0x11, 0x91,
			   0x00,  /* D7-5:VGH_SET_SEL | D4-0:VGL_DET_SEL */
			   0x00,  /* D5-0:VSN_DET_SE */
			   0x00,  /* D5-0:VSP_SET_SE */
			   0x00); /* D7:PUREN_IOVCC
				     D6-4:IOVCC_PUR_SEL
				     D3-2:DCHG1
				     D1-0:DCHG2 */

	/* Set EQ settings */
	dcs_write_seq(dev, 0xE3,  /* SETEQ command */
			   0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00,
			   0x00, 0x00, 0xFF, 0x04, 0xC0, 0x10);

	/* Set GIP */
	dcs_write_seq(dev, 0xE9,  /* SETGIP1 command */
			   0x01,  /* PANEL_SEL */
			   0x00, 0x0E, /* SHR_0 */
			   0x00, 0x00, /* SHR_1 */
			   0xB0, 0xB1, /* SPON SPOFF */
			   0x11, 0x31, /* SHR0_1 SHR0_2 SHR0_3 SHR1_1 */
			   0x23,  /* SHR1_2 SHR1_3 */
			   0x28,  /* SHP SCP */
			   0x10,  /* CHR */
			   0xB0,  /* CON */
			   0xB1,  /* COFF */
			   0x27,  /* CHP CCP */
			   0x08,  /* USER_GIP_GATE */
			   0x00, 0x04, 0x02, /* CGTS_L (Gout2 & 11 = STV) */
			   0x00, 0x00, 0x00, /* CGTS_INV_L */
			   0x00, 0x04, 0x02, /* CGTS_R (Gout2 & 11 = STV) */
			   0x00, 0x00, 0x00, /* CGTS_INV_R */
			   0x88, 0x88, 0xBA, 0x60, /* COS1_L  - COS8_L */
			   0x24, 0x08, 0x88, 0x88, /* COS9_L  - COS16_L */
			   0x88, 0x88, 0x88,       /* COS17_L - COS22_L */
			   0x88, 0x88, 0xBA, 0x71, /* COS1_R  - COS8_R */
			   0x35, 0x18, 0x88, 0x88, /* COS9_R  - COS16_R */
			   0x88, 0x88, 0x88,       /* COS17_R - COS22_R */
			   0x00,  /* TCON_OPT */
			   0x00, 0x00, 0x01, /* GIP_OPT */
			   0x00,  /* CHR2 */
			   0x00,  /* CON2 */
			   0x00,  /* COFF2 */
			   0x00,  /* CHP2 CCP2 */
			   0x00, 0x00, 0x00, /* CKS */
			   0x00,  /* COFF CON SPOFF SPON */
			   0x00); /* COFF2 CON2 PANEL_SEL_INI */

	/* Set GIP2 */
	dcs_write_seq(dev, 0xEA,  /* SETGIP2 command */
			   0x97,  /* YS2_SEL YS1_SEL */
			   0x0A,  /* USER_GIP_GATE1 */
			   0x82,  /* CK_ALL_ON_WIDTH1 */
			   0x02,  /* CK_ALL_ON_WIDTH2 */
			   0x03,  /* CK_ALL_ON_WIDTH3 */
			   0x07,  /* YS_FLAG_PERIOD */
			   0x00,  /* YS2_SEL_2 YS1_SEL_2 */
			   0x00,  /* USER_GIP_GATE1_2 */
			   0x00,  /* CK_ALL_ON_WIDTH1_2 */
			   0x00,  /* CK_ALL_ON_WIDTH2_2 */
			   0x00,  /* CK_ALL_ON_WIDTH3_2 */
			   0x00,  /* YS_FLAG_PERIOD_2 */
			   0x81, 0x88, 0xBA, 0x17, /* COS1_L  - COS8_L */
			   0x53, 0x88, 0x88, 0x88, /* COS9_L  - COS16_L */
			   0x88, 0x88, 0x88,       /* COS17_L - COS22_L */
			   0x80, 0x88, 0xBA, 0x06, /* COS1_R  - COS8_R */
			   0x42, 0x88, 0x88, 0x88, /* COS9_R  - COS16_R */
			   0x88, 0x88, 0x88,       /* COS17_R - COS22_R */
			   0x23,  /* EQOPT EQSEL */
			   0x00,  /* EQ_DELAY */
			   0x00,  /* EQ2_DELAY EQ_DELAY_HSYNC */
			   0x02, 0x80, /* HSYNC_TO_CL1_CNT10 */
			   0x00,  /* HIZ_L */
			   0x00,  /* HIZ_R */
			   0x00, 0x00, 0x00, /* CKS_GS */
			   0x00, 0x00, 0x00, /* CK_MSB_EN */
			   0x00, 0x00, 0x00, /* CK_MSB_EN_GS */
			   0x00, 0x00, /* SHR2 */
			   0x00, 0x00, /* SHR2_1 SHR2_2 SHR2_3 */
			   0x00, 0x00, 0x00, /* SHP1 SPON1 SPOFF1 */
			   0x00, 0x00, 0x00, /* SHP2 SPON2 SPOFF2 */
			   0x00);  /* SPOFF2 SPON2 SPOFF1 SPON1 */

	dcs_write_seq(dev, 0xEF,
			   0xFF, 0xFF, 0x01);

	/* Sleep Out */
	dcs_write_seq(dev, MIPI_DCS_EXIT_SLEEP_MODE);
	mdelay(250);
}

static int nhd35640480ef_panel_enable_backlight(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct nhd35640480ef_panel_priv *priv = dev_get_priv(dev);
	int ret;

	ret = mipi_dsi_attach(device);
	if (ret < 0)
		return ret;

	/* Reset panel */
	dm_gpio_set_value(&priv->reset, 0);
	mdelay(20);
	dm_gpio_set_value(&priv->reset, 1);
	mdelay(150);

	/* Configure panel controller */
	nhd35640480ef_read_id(dev);
	nhd35640480ef_init_sequence(dev);

	/* Enable display*/
	mipi_dsi_dcs_set_display_on(device);
	mdelay(50);

	return 0;
}

static int nhd35640480ef_panel_get_display_timing(struct udevice *dev,
					     struct display_timing *timings)
{
	struct nhd35640480ef_panel_priv *priv = dev_get_priv(dev);
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;

	memcpy(timings, &default_timing, sizeof(*timings));

	/* fill characteristics of DSI data link */
	if (device) {
		device->lanes = priv->lanes;
		device->format = priv->format;
		device->mode_flags = priv->mode_flags;
	}

	return 0;
}

static int nhd35640480ef_panel_probe(struct udevice *dev)
{
	struct nhd35640480ef_panel_priv *priv = dev_get_priv(dev);
	int ret;

	if (IS_ENABLED(CONFIG_DM_REGULATOR)) {
		ret =  device_get_supply_regulator(dev, "VCC-supply",
						   &priv->vcc_reg);
		if (ret)
			dev_err(dev, "Warning: cannot get VCC supply\n");

		if (!ret && priv->vcc_reg) {
			dev_dbg(dev, "enable regulator '%s'\n", priv->vcc_reg->name);
			ret = regulator_set_enable(priv->vcc_reg, true);
			if (ret) {
				dev_err(dev, "Error: cannot enable VCC supply\n");
				return ret;
			}
		}

		ret =  device_get_supply_regulator(dev, "IOVCC-supply",
						   &priv->iovcc_reg);
		if (ret)
			dev_err(dev, "Warning: cannot get IOVCC supply\n");

		if (!ret && priv->iovcc_reg) {
			dev_dbg(dev, "enable regulator '%s'\n", priv->iovcc_reg->name);
			ret = regulator_set_enable(priv->iovcc_reg, true);
			if (ret) {
				dev_err(dev, "Error: cannot enable IOVCC supply\n");
				return ret;
			}
		}
	}

	/* fill characteristics of DSI data link */
	priv->format = MIPI_DSI_FMT_RGB888;
	priv->mode_flags = MIPI_DSI_MODE_VIDEO |
			  MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_LPM |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset,
				   GPIOD_IS_OUT);
	if (ret) {
		dev_err(dev, "Warning: cannot get reset GPIO\n");
		if (ret != -ENOENT)
			return ret;
	}

	ret = dev_read_u32(dev, "dsi-lanes", &priv->lanes);
	if (ret) {
		dev_err(dev, "Warning: cannot get dsi-lanes property\n");
		if (ret != -ENOENT)
			return ret;
	}

	return 0;
}

static const struct panel_ops nhd35640480ef_panel_ops = {
	.enable_backlight = nhd35640480ef_panel_enable_backlight,
	.get_display_timing = nhd35640480ef_panel_get_display_timing,
};

static const struct udevice_id nhd35640480ef_panel_ids[] = {
	{ .compatible = "newhaven,nhd-3.5-640480ef-msxp" },
	{ }
};

U_BOOT_DRIVER(nhd35640480ef_panel) = {
	.name           = "nhd35640480ef_panel",
	.id             = UCLASS_PANEL,
	.of_match       = nhd35640480ef_panel_ids,
	.ops            = &nhd35640480ef_panel_ops,
	.probe          = nhd35640480ef_panel_probe,
	.plat_auto      = sizeof(struct mipi_dsi_panel_plat),
	.priv_auto      = sizeof(struct nhd35640480ef_panel_priv),
};