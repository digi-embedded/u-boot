// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2023 NXP
 * Copyright (C) 2026, Digi International Inc.
 */
#include <common.h>
#include <clk.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <display.h>
#include <video.h>
#include <video_bridge.h>
#include <video_link.h>
#include <asm/io.h>
#include <dm/device-internal.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <clk.h>

#include <power-domain.h>
#include <syscon.h>
#include <media_bus_format.h>
#include <panel.h>

#define DRIVER_NAME "imx93-ldb"


#define LDB_CTRL		0x20
#define LDB_CH0_EN		BIT(0)
#define LDB_DATA_WIDTH_CH0_24	BIT(5)
#define LDB_BIT_MAP_CH0_JEIDA	BIT(6)

#define LVDS_CTRL		0x24
#define SPARE_IN(n)		(((n) & 0x7) << 25)
#define SPARE_IN_MASK		0xe000000
#define TEST_RANDOM_NUM_EN	BIT(24)
#define TEST_MUX_SRC(n)		(((n) & 0x3) << 22)
#define TEST_MUX_SRC_MASK	0xc00000
#define TEST_EN			BIT(21)
#define TEST_DIV4_EN		BIT(20)
#define VBG_ADJ(n)		(((n) & 0x7) << 17)
#define VBG_ADJ_MASK		0xe0000
#define SLEW_ADJ(n)		(((n) & 0x7) << 14)
#define SLEW_ADJ_MASK		0x1c000
#define CC_ADJ(n)		(((n) & 0x7) << 11)
#define CC_ADJ_MASK		0x3800
#define CM_ADJ(n)		(((n) & 0x7) << 8)
#define CM_ADJ_MASK		0x700
#define PRE_EMPH_ADJ(n)		(((n) & 0x7) << 5)
#define PRE_EMPH_ADJ_MASK	0xe0
#define PRE_EMPH_EN		BIT(4)
#define HS_EN			BIT(3)
#define BG_EN			BIT(2)
#define DISABLE_LVDS		BIT(1)
#define CH_EN(id)		BIT(id)

#define MAX_LDB_CHAN_NUM	1

struct imx93_ldb_channel {
	u32 chno;
	bool is_available;
	u32 out_bus_format;
	/*struct phy phy;*/
};

struct imx93_ldb {
	void __iomem *base;
	struct udevice *conn_dev;
	struct imx93_ldb_channel channel;
	struct clk clk_ldb;
	unsigned int ctrl_reg;
	u32 ldb_ctrl;
	unsigned int available_ch_cnt;
	struct display_timing timings;
	/* phy parameters */
	u32 lvds_ctrl;
	bool has_disable;
};

static inline unsigned int media_blk_read(struct imx93_ldb *priv, unsigned int reg)
{
	unsigned int val;
	readsl(priv->base + reg, &val, 32);

	return val;
}

static inline void media_blk_write(struct imx93_ldb *priv, unsigned int reg, unsigned int value)
{
	setbits_le32(priv->base + reg, value);
}

/* function mirrors 'imx8mp_lvds_phy_init' */
static int imx93_lvds_phy_init(struct udevice *dev)
{
	struct imx93_ldb *priv;
	ulong drv_data = dev_get_driver_data(dev);

	/* Call from parent phy node should be invalid */
	if (drv_data)
		return -EINVAL;

	priv = dev_get_priv(dev_get_parent(dev));

	media_blk_write(priv, priv->lvds_ctrl,
			CC_ADJ(0x2) | PRE_EMPH_EN | PRE_EMPH_ADJ(0x3));

	return 0;
}

/* function mirrors 'imx8mp_lvds_phy_power_on' */
static int imx93_lvds_phy_power_on(struct udevice *dev)
{
	struct imx93_ldb *priv;
	ulong drv_data = dev_get_driver_data(dev);
	u32 id, val;
	bool bg_en;

	/* Call from parent phy node should be invalid */
	if (drv_data)
		return -EINVAL;

	id = (ulong)dev_get_plat(dev);
	priv = dev_get_priv(dev_get_parent(dev));

	val = media_blk_read(priv, priv->lvds_ctrl);
	bg_en = !!(val & BG_EN);
	val |= BG_EN;
	if (priv->has_disable)
		val &= ~DISABLE_LVDS;
	media_blk_write(priv, priv->lvds_ctrl, val);

	/* Wait 15us to make sure the bandgap to be stable. */
	if (!bg_en)
		udelay(20);

	val = media_blk_read(priv, priv->lvds_ctrl);
	val |= CH_EN(id);
	media_blk_write(priv, priv->lvds_ctrl, val);

	/* Wait 5us to ensure the phy be settling. */
	udelay(10);

	return 0;
}

static void imx93_lvds_mode_set(struct udevice *dev)
{
	struct imx93_ldb *imx93_ldb = dev_get_priv(dev_get_parent(dev));
	struct imx93_ldb_channel *imx93_ldb_ch = &imx93_ldb->channel;
	int ret;

	ret = imx93_lvds_phy_init(dev);
	if (ret < 0)
		dev_err(dev, "failed to initialize PHY: %d\n", ret);

	debug("ldb out_bus format 0%x\n", imx93_ldb_ch->out_bus_format);

	imx93_ldb->ldb_ctrl |= LDB_CH0_EN;

	switch (imx93_ldb_ch->out_bus_format) {
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
	default:
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
		imx93_ldb->ldb_ctrl |= LDB_DATA_WIDTH_CH0_24;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
		imx93_ldb->ldb_ctrl |= LDB_DATA_WIDTH_CH0_24 | LDB_BIT_MAP_CH0_JEIDA;
		break;
	}
}

static void imx93_ldb_enable(struct udevice *dev)
{
	struct imx93_ldb *imx93_ldb = dev_get_priv(dev_get_parent(dev));
	int ret;

	ret = imx93_lvds_phy_power_on(dev);
	if (ret)
		dev_err(dev, "failed to power on PHY: %d\n", ret);

	media_blk_write(imx93_ldb, imx93_ldb->ctrl_reg, imx93_ldb->ldb_ctrl);
}

int imx93_ldb_read_timing(struct udevice *dev, struct display_timing *timing)
{
	struct imx93_ldb *priv = dev_get_priv(dev);
	ulong drv_data = dev_get_driver_data(dev);

	if (drv_data)
		return -EINVAL;

	if (timing) {
		memcpy(timing, &priv->timings, sizeof(struct display_timing));
		return 0;
	}

	return -EINVAL;
}

int imx93_ldb_enable_display(struct udevice *dev, int panel_bpp,
			     const struct display_timing *timing)
{
	struct imx93_ldb *priv = dev_get_priv(dev);
	ulong drv_data = dev_get_driver_data(dev);
	int ret;

	debug("%s\n", __func__);

	if (drv_data)
		return -EINVAL;

	imx93_lvds_mode_set(dev);
	imx93_ldb_enable(dev);

	if (IS_ENABLED(CONFIG_VIDEO_BRIDGE)) {
		if (priv->conn_dev &&
			device_get_uclass_id(priv->conn_dev) == UCLASS_VIDEO_BRIDGE) {
			ret = video_bridge_set_backlight(priv->conn_dev, 80);
			if (ret) {
				dev_err(dev, "fail to set backlight\n");
				return ret;
			}
		}
	}

	if (IS_ENABLED(CONFIG_PANEL)) {
		if (priv->conn_dev &&
			device_get_uclass_id(priv->conn_dev) == UCLASS_PANEL) {
			ret = panel_enable_backlight(priv->conn_dev);
			if (ret) {
				dev_err(dev, "fail to set backlight\n");
				return ret;
			}
		}
	}

	return 0;
}

static int of_get_bus_format(struct udevice *dev, u32 *bus_format)
{
	const char *bm;
	int ret;
	u32 dw;

	bm = dev_read_string(dev, "fsl,data-mapping");
	if (bm == NULL)
		return -EINVAL;

	ret = dev_read_u32(dev, "fsl,data-width", &dw);
	if (ret || (dw != 18 && dw != 24)) {
		printf("data width not set or invalid\n");
		return -EINVAL;
	}

	if (!strcasecmp(bm, "spwg") && dw == 18)
		*bus_format = MEDIA_BUS_FMT_RGB666_1X7X3_SPWG;
	else if (!strcasecmp(bm, "spwg") && dw == 24)
		*bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_SPWG;
	else if (!strcasecmp(bm, "jeida") && dw == 24)
		*bus_format = MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA;
	else
		return -EINVAL;

	return 0;
}

static int imx93_ldb_probe(struct udevice *dev)
{
	struct imx93_ldb *priv = dev_get_priv(dev);
	ulong drv_data = dev_get_driver_data(dev);
	int ret;
	ofnode lvds_ch_node;
	u32 ch_id;
	const char *stat;

	debug("%s\n", __func__);

	if (!drv_data) { /* sub node */
		struct imx93_ldb *priv_parent = dev_get_priv(dev_get_parent(dev));

		priv->conn_dev = video_link_get_next_device(dev);
		if (!priv->conn_dev) {
			dev_err(dev, "can't find next device in video link\n");
			return -ENODEV;
		}

		ret = of_get_bus_format(dev, &priv_parent->channel.out_bus_format);
		if (ret) {
			dev_err(dev, "can't find property for bus format\n");
			return ret;
		}

		debug("ldb channel bus format 0x%x\n", priv_parent->channel.out_bus_format);

		ret = video_link_get_display_timings(&priv->timings);
		if (ret) {
			dev_err(dev, "decode display timing error %d\n", ret);
			return ret;
		}

		if (IS_ENABLED(CONFIG_VIDEO_BRIDGE)) {
			if (priv->conn_dev &&
				device_get_uclass_id(priv->conn_dev) == UCLASS_VIDEO_BRIDGE) {
				ret = video_bridge_attach(priv->conn_dev);
				if (ret) {
					dev_err(dev, "fail to attach bridge\n");
					return ret;
				}
				ret = video_bridge_set_active(priv->conn_dev, true);
				if (ret) {
					dev_err(dev, "fail to active bridge\n");
					return ret;
				}
			}
		}

		return 0;
	}

	/* parent node */
	ret = clk_get_by_name(dev, "ldb", &priv->clk_ldb);
	if (ret) {
		dev_err(dev, "failed to get clock ldb\n");
		return ret;
	}

	priv->base = (void *)0x4ac10000; /* media_blk_ctrl base address*/

	priv->ctrl_reg = LDB_CTRL;
	priv->ldb_ctrl = 0;

	priv->lvds_ctrl = LVDS_CTRL;
	priv->has_disable = true;

	ofnode_for_each_subnode(lvds_ch_node, dev_ofnode(dev)) {
		if (ofnode_read_u32(lvds_ch_node, "reg", &ch_id)) {
			dev_err(dev, "missing reg property in node %s\n",
				ofnode_get_name(lvds_ch_node));
			return -EINVAL;
		}

		if (ch_id >= MAX_LDB_CHAN_NUM) {
			dev_err(dev, "invalid reg in node %s\n", ofnode_get_name(lvds_ch_node));
			return -EINVAL;
		}

		stat = ofnode_read_string(lvds_ch_node, "status");
		if (stat && strcmp(stat, "okay"))
			continue;

		/*ret = generic_phy_get_by_index_nodev(lvds_ch_node, 0, &priv->channel.phy);
		if (ret) {
			dev_err(dev, "fail to get phy device\n");
			return -EINVAL;
		}*/

		priv->channel.chno = ch_id;
		priv->channel.is_available = true;

		priv->available_ch_cnt++;
	}

	if (priv->available_ch_cnt == 0) {
		dev_dbg(dev, "no available channel\n");
		return 0;
	}

	return 0;

}

static int imx93_ldb_bind(struct udevice *dev)
{
	ofnode lvds_ch_node;
	u32 ch_id;
	ulong drv_data = dev_get_driver_data(dev);
	int ret = 0;

	debug("%s\n", __func__);

	if (drv_data) {
		/* Parent lvds phy node, bind each subnode to driver */
		ofnode_for_each_subnode(lvds_ch_node, dev_ofnode(dev)) {
			if (ofnode_read_u32(lvds_ch_node, "reg", &ch_id)) {
				dev_err(dev, "missing reg property in node %s\n",
					ofnode_get_name(lvds_ch_node));
				return -EINVAL;
			}

			if (ch_id >= MAX_LDB_CHAN_NUM) {
				dev_err(dev, "invalid reg in node %s\n", ofnode_get_name(lvds_ch_node));
				return -EINVAL;
			}

			ret = device_bind(dev, dev->driver, ofnode_get_name(lvds_ch_node),
				(void *)(ulong)ch_id, lvds_ch_node, NULL);
			if (ret)
				printf("Error binding driver '%s': %d\n", dev->driver->name,
					ret);
		}
	}

	return ret;
}

struct dm_display_ops imx93_ldb_ops = {
	.read_timing = imx93_ldb_read_timing,
	.enable = imx93_ldb_enable_display,
};

static const struct udevice_id imx93_ldb_ids[] = {
	{ .compatible = "fsl,imx93-ldb", .data = 1 }, /* data is used to indicate parent device */
	{ }
};

U_BOOT_DRIVER(imx93_ldb) = {
	.name			= "imx93_ldb",
	.id			= UCLASS_DISPLAY,
	.of_match		= imx93_ldb_ids,
	.bind			= imx93_ldb_bind,
	.probe			= imx93_ldb_probe,
	.ops			= &imx93_ldb_ops,
	.priv_auto		= sizeof(struct imx93_ldb),
	.flags			= DM_FLAG_DEFAULT_PD_CTRL_OFF,
};