// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2024 Digi International
 *
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <linux/delay.h>
#include <linux/err.h>
#if CONFIG_IS_ENABLED(DM_REGULATOR)
#include <power/regulator.h>
#endif

#define LT8912_REG_CHIP_REVISION_0                 0x00
#define LT8912_REG_CHIP_REVISION_0_VALUE           0x12

#define LT8912_REG_CHIP_REVISION_1                 0x01
#define LT8912_REG_CHIP_REVISION_1_VALUE           0xb2

/* Video mode flags */
/* bit compatible with the xrandr RR_ definitions (bits 0-13)
 *
 * ABI warning: Existing userspace really expects
 * the mode flags to match the xrandr definitions. Any
 * changes that don't match the xrandr definitions will
 * likely need a new client cap or some other mechanism
 * to avoid breaking existing userspace. This includes
 * allocating new flags in the previously unused bits!
 */
#define DRM_MODE_FLAG_PHSYNC                    (1<<0)
#define DRM_MODE_FLAG_NHSYNC                    (1<<1)
#define DRM_MODE_FLAG_PVSYNC                    (1<<2)
#define DRM_MODE_FLAG_NVSYNC                    (1<<3)
#define DRM_MODE_FLAG_INTERLACE                 (1<<4)
#define DRM_MODE_FLAG_DBLSCAN                   (1<<5)
#define DRM_MODE_FLAG_CSYNC                     (1<<6)
#define DRM_MODE_FLAG_PCSYNC                    (1<<7)
#define DRM_MODE_FLAG_NCSYNC                    (1<<8)
#define DRM_MODE_FLAG_HSKEW                     (1<<9) /* hskew provided */
#define DRM_MODE_FLAG_BCAST                     (1<<10) /* deprecated */
#define DRM_MODE_FLAG_PIXMUX                    (1<<11) /* deprecated */
#define DRM_MODE_FLAG_DBLCLK                    (1<<12)
#define DRM_MODE_FLAG_CLKDIV2                   (1<<13)

#define LT_SINK_IS_HDMI         0
#define MODE_FLAGS_HDMI         5


struct lt8912_priv {
	unsigned int addr;
	unsigned int addr_p1;
	unsigned int addr_p2;
	unsigned int lanes;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
#if CONFIG_IS_ENABLED(DM_REGULATOR)
	struct udevice *vdd1;
	struct udevice *vdd2;
#endif
	struct udevice *p1_dev;
	struct udevice *p2_dev;
};

static const struct display_timing default_timing = {
	.pixelclock.typ		= 148500000,
	.hactive.typ		= 1920,
	.hfront_porch.typ	= 88,
	.hback_porch.typ	= 148,
	.hsync_len.typ		= 44,
	.vactive.typ		= 1080,
	.vfront_porch.typ	= 36,
	.vback_porch.typ	= 4,
	.vsync_len.typ		= 5,
};

static int lt8912_i2c_reg_write(struct udevice *dev, uint addr, uint data)
{
	uint8_t valb;
	int err;

	valb = data;

	err = dm_i2c_write(dev, addr, &valb, 1);
	return err;
}

static int lt8912_i2c_reg_read(struct udevice *dev, uint8_t addr, uint8_t *data)
{
	uint8_t valb;
	int err;

	err = dm_i2c_read(dev, addr, &valb, 1);
	if (err)
		return err;

	*data = (int)valb;
	return 0;
}

static int lt8912_wakeup(struct udevice *dev)
{
	//TODO: Linux driver does a 120mS low pulse to LT8912 RST#

	lt8912_i2c_reg_write(dev, 0x08, 0xff);
	lt8912_i2c_reg_write(dev, 0x41, 0x3f);
	lt8912_i2c_reg_write(dev, 0x05, 0xfb);
	lt8912_i2c_reg_write(dev, 0x05, 0xff);
	lt8912_i2c_reg_write(dev, 0x03, 0x7f);

	mdelay(15);

	lt8912_i2c_reg_write(dev, 0x03, 0xff);
	lt8912_i2c_reg_write(dev, 0x32, 0xa1);
	lt8912_i2c_reg_write(dev, 0x33, 0x03);

	mdelay(150);

	return 0;
}

static int lt8912_enable(struct udevice *dev)
{
	struct lt8912_priv *priv = dev_get_priv(dev);
	uint8_t val;
	uint8_t lanes;
	uint32_t hactive, hfp, hsync, hbp, vfp, vsync, vbp, htotal, vtotal;
	unsigned int hsync_activehigh, vsync_activehigh;

	lanes = priv->lanes;
	hactive = default_timing.hactive.typ;
	hfp = default_timing.hfront_porch.typ;
	hsync = default_timing.hsync_len.typ;
	hsync_activehigh = !!(MODE_FLAGS_HDMI & DRM_MODE_FLAG_PHSYNC);
	hbp = default_timing.hback_porch.typ;
	vfp = default_timing.vfront_porch.typ;
	vsync = default_timing.vsync_len.typ;
	vsync_activehigh = !!(MODE_FLAGS_HDMI & DRM_MODE_FLAG_PVSYNC);
	vbp = default_timing.vback_porch.typ;
	htotal = hactive + hfp + hsync + hbp;
	vtotal = default_timing.vactive.typ + vfp + vsync + vbp;

	lt8912_i2c_reg_read(dev, LT8912_REG_CHIP_REVISION_0, &val);
	debug("Chip revision 0: 0x%x (expected: 0x%x)\n", val, LT8912_REG_CHIP_REVISION_0_VALUE);
	lt8912_i2c_reg_read(dev, LT8912_REG_CHIP_REVISION_1, &val);
	debug("Chip revision 1: 0x%x (expected: 0x%x)\n", val, LT8912_REG_CHIP_REVISION_1_VALUE);

	/* DigitalClockEn */
	lt8912_i2c_reg_write(dev, 0x08, 0xff);
	lt8912_i2c_reg_write(dev, 0x09, 0xff);
	lt8912_i2c_reg_write(dev, 0x0a, 0xff);
	lt8912_i2c_reg_write(dev, 0x0b, 0x7c);
	lt8912_i2c_reg_write(dev, 0x0c, 0xff);

	/* TxAnalog */
	lt8912_i2c_reg_write(dev, 0x31, 0xa1);
	lt8912_i2c_reg_write(dev, 0x32, 0xa1);
	lt8912_i2c_reg_write(dev, 0x33, 0x03);
	lt8912_i2c_reg_write(dev, 0x37, 0x00);
	lt8912_i2c_reg_write(dev, 0x38, 0x22);
	lt8912_i2c_reg_write(dev, 0x60, 0x82);

	/* CbusAnalog */
	lt8912_i2c_reg_write(dev, 0x39, 0x45);
	lt8912_i2c_reg_write(dev, 0x3a, 0x00);
	lt8912_i2c_reg_write(dev, 0x3b, 0x00);

	/* HDMIPllAnalog */
	lt8912_i2c_reg_write(dev, 0x44, 0x31);
	lt8912_i2c_reg_write(dev, 0x55, 0x44);
	lt8912_i2c_reg_write(dev, 0x57, 0x01);
	lt8912_i2c_reg_write(dev, 0x5a, 0x02);

	/* MIPIAnalog */
	lt8912_i2c_reg_write(dev, 0x3e, 0xce);
	lt8912_i2c_reg_write(dev, 0x3f, 0xd4);
	lt8912_i2c_reg_write(dev, 0x41, 0x3c);

	/* MipiBasicSet */
	lt8912_i2c_reg_write(priv->p1_dev, 0x12, 0x04);
	lt8912_i2c_reg_write(priv->p1_dev, 0x13, lanes % 4);
	lt8912_i2c_reg_write(priv->p1_dev, 0x14, 0x00);

	lt8912_i2c_reg_write(priv->p1_dev, 0x15, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x1a, 0x03);
	lt8912_i2c_reg_write(priv->p1_dev, 0x1b, 0x03);

	/* MIPIDig */
	lt8912_i2c_reg_write(priv->p1_dev, 0x10, 0x01);
	lt8912_i2c_reg_write(priv->p1_dev, 0x11, 0x0a);
	lt8912_i2c_reg_write(priv->p1_dev, 0x18, hsync);
	lt8912_i2c_reg_write(priv->p1_dev, 0x19, vsync);
	lt8912_i2c_reg_write(priv->p1_dev, 0x1c, hactive % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x1d, hactive >> 8);

	lt8912_i2c_reg_write(priv->p1_dev, 0x2f, 0x0c);

	lt8912_i2c_reg_write(priv->p1_dev, 0x34, htotal % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x35, htotal >> 8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x36, vtotal % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x37, vtotal >> 8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x38, vbp % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x39, vbp >> 8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3a, vfp % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3b, vfp >> 8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3c, hbp % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3d, hbp >> 8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3e, hfp % 0x100);
	lt8912_i2c_reg_write(priv->p1_dev, 0x3f, hfp >> 8);
	lt8912_i2c_reg_read(dev, 0xab, &val);
	val &= 0xfc;
	val |= (hsync_activehigh << 1) | vsync_activehigh;
	lt8912_i2c_reg_write(dev, 0xab, val);

	/* DDSConfig */
	lt8912_i2c_reg_write(priv->p1_dev, 0x4e, 0x6a);
	lt8912_i2c_reg_write(priv->p1_dev, 0x4f, 0xad);
	lt8912_i2c_reg_write(priv->p1_dev, 0x50, 0xf3);
	lt8912_i2c_reg_write(priv->p1_dev, 0x51, 0x80);

	lt8912_i2c_reg_write(priv->p1_dev, 0x1f, 0x5e);
	lt8912_i2c_reg_write(priv->p1_dev, 0x20, 0x01);
	lt8912_i2c_reg_write(priv->p1_dev, 0x21, 0x2c);
	lt8912_i2c_reg_write(priv->p1_dev, 0x22, 0x01);
	lt8912_i2c_reg_write(priv->p1_dev, 0x23, 0xfa);
	lt8912_i2c_reg_write(priv->p1_dev, 0x24, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x25, 0xc8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x26, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x27, 0x5e);
	lt8912_i2c_reg_write(priv->p1_dev, 0x28, 0x01);
	lt8912_i2c_reg_write(priv->p1_dev, 0x29, 0x2c);
	lt8912_i2c_reg_write(priv->p1_dev, 0x2a, 0x01);
	lt8912_i2c_reg_write(priv->p1_dev, 0x2b, 0xfa);
	lt8912_i2c_reg_write(priv->p1_dev, 0x2c, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x2d, 0xc8);
	lt8912_i2c_reg_write(priv->p1_dev, 0x2e, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x42, 0x64);
	lt8912_i2c_reg_write(priv->p1_dev, 0x43, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x44, 0x04);
	lt8912_i2c_reg_write(priv->p1_dev, 0x45, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x46, 0x59);
	lt8912_i2c_reg_write(priv->p1_dev, 0x47, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x48, 0xf2);
	lt8912_i2c_reg_write(priv->p1_dev, 0x49, 0x06);
	lt8912_i2c_reg_write(priv->p1_dev, 0x4a, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x4b, 0x72);
	lt8912_i2c_reg_write(priv->p1_dev, 0x4c, 0x45);
	lt8912_i2c_reg_write(priv->p1_dev, 0x4d, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x52, 0x08);
	lt8912_i2c_reg_write(priv->p1_dev, 0x53, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x54, 0xb2);
	lt8912_i2c_reg_write(priv->p1_dev, 0x55, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x56, 0xe4);
	lt8912_i2c_reg_write(priv->p1_dev, 0x57, 0x0d);
	lt8912_i2c_reg_write(priv->p1_dev, 0x58, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x59, 0xe4);
	lt8912_i2c_reg_write(priv->p1_dev, 0x5a, 0x8a);
	lt8912_i2c_reg_write(priv->p1_dev, 0x5b, 0x00);
	lt8912_i2c_reg_write(priv->p1_dev, 0x5c, 0x34);
	lt8912_i2c_reg_write(priv->p1_dev, 0x1e, 0x4f);
	lt8912_i2c_reg_write(priv->p1_dev, 0x51, 0x00);

	lt8912_i2c_reg_write(dev, 0xb2, LT_SINK_IS_HDMI);

	/* Audio Disable */
	lt8912_i2c_reg_write(priv->p2_dev, 0x06, 0x00);
	lt8912_i2c_reg_write(priv->p2_dev, 0x07, 0x00);

	lt8912_i2c_reg_write(priv->p2_dev, 0x34, 0xd2);

	lt8912_i2c_reg_write(priv->p2_dev, 0x3c, 0x41);

	/* MIPIRxLogicRes */
	lt8912_i2c_reg_write(dev, 0x03, 0x7f);
	mdelay(15);
	lt8912_i2c_reg_write(dev, 0x03, 0xff);

	lt8912_i2c_reg_write(priv->p1_dev, 0x51, 0x80);
	mdelay(15);
	lt8912_i2c_reg_write(priv->p1_dev, 0x51, 0x00);

	return 0;
}

static int lt8912_enable_backlight(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	int ret;

	ret = mipi_dsi_attach(device);
	if (ret < 0)
		return ret;

	return 0;
}

static int lt8912_get_display_timing(struct udevice *dev,
					    struct display_timing *timings)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct lt8912_priv *priv = dev_get_priv(dev);

	memcpy(timings, &default_timing, sizeof(*timings));

	/* fill characteristics of DSI data link */
	if (device) {
		device->lanes = priv->lanes;
		device->format = priv->format;
		device->mode_flags = priv->mode_flags;
	}

	return 0;
}

static int lt8912_probe(struct udevice *dev)
{
	struct lt8912_priv *priv = dev_get_priv(dev);
	int ret;

	debug("%s\n", __func__);

	priv->format = MIPI_DSI_FMT_RGB888;
	priv->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_SYNC_PULSE |
			  MIPI_DSI_MODE_EOT_PACKET | MIPI_DSI_MODE_VIDEO_HSE,

	priv->addr  = dev_read_addr(dev);
	if (priv->addr  == 0)
		return -ENODEV;

	ret = dev_read_u32(dev, "adi,dsi-lanes", &priv->lanes);
	if (ret) {
		dev_err(dev, "Failed to get dsi-lanes property (%d)\n", ret);
		return ret;
	}

	if (priv->lanes < 1 || priv->lanes > 4) {
		dev_err(dev, "Invalid dsi-lanes: %d\n", priv->lanes);
		return -EINVAL;
	}

#if CONFIG_IS_ENABLED(DM_REGULATOR)
	ret = device_get_supply_regulator(dev, "vdd1-supply",
					  &priv->vdd1);
	if (ret) {
		debug("%s: No vdd1 supply\n", dev->name);
		dev_err(dev, "%s: No vdd1 supply\n", dev->name);
	} else {
		dev_err(dev, "%s: vdd1 supply enabled\n", dev->name);
	}

	if (!ret && priv->vdd1) {
		ret = regulator_set_enable(priv->vdd1, true);
		if (ret) {
			puts("Error enabling vdd1 supply\n");
			return ret;
		}
	}

	ret = device_get_supply_regulator(dev, "vdd2-supply",
					  &priv->vdd2);
	if (ret) {
		debug("%s: No vdd2 supply\n", dev->name);
		dev_err(dev, "%s: No vdd2 supply\n", dev->name);
	} else {
		dev_err(dev, "%s: vdd2 supply enabled\n", dev->name);
	}

	if (!ret && priv->vdd2) {
		ret = regulator_set_enable(priv->vdd2, true);
		if (ret) {
			puts("Error enabling vdd2 supply\n");
			return ret;
		}
	}
#endif

	lt8912_wakeup(dev);

	priv->addr_p1 = priv->addr + 1;
	ret = dm_i2c_probe(dev_get_parent(dev), priv->addr_p1, 0, &priv->p1_dev);
	if (ret) {
		dev_err(dev, "Can't find p1 device id=0x%x\n", priv->addr_p1);
		return -ENODEV;
	}
	priv->addr_p2 = priv->addr + 2;
	ret = dm_i2c_probe(dev_get_parent(dev), priv->addr_p2, 0, &priv->p2_dev);
	if (ret) {
		dev_err(dev, "Can't find p2 device id=0x%x\n", priv->addr_p2);
		return -ENODEV;
	}

	lt8912_enable(dev);

	return 0;
}

static const struct panel_ops lt8912_ops = {
	.enable_backlight = lt8912_enable_backlight,
	.get_display_timing = lt8912_get_display_timing,
};

static const struct udevice_id lt8912_ids[] = {
	{ .compatible = "lontium,lt8912" },
	{ }
};

U_BOOT_DRIVER(lt8912_mipi2hdmi) = {
	.name			  = "lt8912_mipi2hdmi",
	.id			  = UCLASS_PANEL,
	.of_match		  = lt8912_ids,
	.ops			  = &lt8912_ops,
	.probe			  = lt8912_probe,
	.plat_auto = sizeof(struct mipi_dsi_panel_plat),
	.priv_auto	= sizeof(struct lt8912_priv),
};
