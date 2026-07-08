// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2026 NXP
 *
 */

#include <dm.h>
#include <dm/device_compat.h>
#include <mipi_dsi.h>
#include <panel.h>
#include <asm/gpio.h>
#include <i2c.h>
#include <linux/err.h>

struct ws_priv {
	unsigned int addr;
	enum mipi_dsi_pixel_format format;
	unsigned long mode_flags;
	unsigned int lanes;
};

static const struct display_timing default_timing = {
	.pixelclock.typ		= 50000000,
	.hactive.typ		= 1024,
	.hfront_porch.typ	= 100,
	.hback_porch.typ	= 100,
	.hsync_len.typ		= 100,
	.vactive.typ		= 600,
	.vfront_porch.typ	= 10,
	.vback_porch.typ	= 10,
	.vsync_len.typ		= 10,
};

static int ws_i2c_reg_write(struct udevice *dev, uint addr, uint mask, uint data)
{
	uint8_t valb;
	int err;

	if (mask != 0xff) {
		err = dm_i2c_read(dev, addr, &valb, 1);
		if (err)
			return err;

		valb &= ~mask;
		valb |= data;
	} else {
		valb = data;
	}

	err = dm_i2c_write(dev, addr, &valb, 1);
	return err;
}

static int ws_enable(struct udevice *dev)
{
	ws_i2c_reg_write(dev, 0xad, 0xff, 0x01);

	return 0;
}

static int ws_disable(struct udevice *dev)
{
	ws_i2c_reg_write(dev, 0xad, 0xff, 0x00);

	return 0;
}

static int ws_enable_backlight(struct udevice *dev)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	int ret;

	ws_i2c_reg_write(dev, 0xab, 0xff, 0x0); /* max bright */
	ws_i2c_reg_write(dev, 0xaa, 0xff, 0x01);

	ws_enable(dev);

	ret = mipi_dsi_attach(device);
	if (ret < 0)
		return ret;

	return 0;
}

static int ws_get_display_timing(struct udevice *dev,
					    struct display_timing *timings)
{
	struct mipi_dsi_panel_plat *plat = dev_get_plat(dev);
	struct mipi_dsi_device *device = plat->device;
	struct ws_priv *priv = dev_get_priv(dev);

	memcpy(timings, &default_timing, sizeof(*timings));

	/* fill characteristics of DSI data link */
	if (device) {
		device->lanes = priv->lanes;
		device->format = priv->format;
		device->mode_flags = priv->mode_flags;
	}

	return 0;
}

static int ws_probe(struct udevice *dev)
{
	struct ws_priv *priv = dev_get_priv(dev);

	debug("%s\n", __func__);

	priv->format = MIPI_DSI_FMT_RGB888;
	priv->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_VIDEO_HSE;
	priv->lanes = 2;

	priv->addr  = dev_read_addr(dev);
	if (priv->addr  == 0)
		return -ENODEV;

	ws_i2c_reg_write(dev, 0xc0, 0xff, 0x01);
	ws_i2c_reg_write(dev, 0xc2, 0xff, 0x01);
	ws_i2c_reg_write(dev, 0xac, 0xff, 0x01);

	return 0;
}

static int ws_remove(struct udevice *dev)
{
	ws_disable(dev);

	return 0;
}

static const struct panel_ops ws_ops = {
	.enable_backlight = ws_enable_backlight,
	.get_display_timing = ws_get_display_timing,
};

static const struct udevice_id ws_ids[] = {
	{ .compatible = "waveshare,7.0inch-c-panel" },
	{ }
};

U_BOOT_DRIVER(ws_panel) = {
	.name			  = "ws_panel",
	.id			  = UCLASS_PANEL,
	.of_match		  = ws_ids,
	.ops			  = &ws_ops,
	.probe			  = ws_probe,
	.remove			  = ws_remove,
	.plat_auto = sizeof(struct mipi_dsi_panel_plat),
	.priv_auto	= sizeof(struct ws_priv),
};
