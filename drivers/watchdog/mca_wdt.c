/*
 *  Copyright 2021 Digi International Inc
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <hang.h>
#include <asm/io.h>
#include <wdt.h>
#include <watchdog.h>
#include "../../board/digi/common/mca_registers.h"
#include "../../board/digi/common/mca.h"

#define WDT_REFRESH_LEN		(MCA_WDT_REFRESH_3 - \
				 MCA_WDT_REFRESH_0 + 1)
#define WDT_REFRESH_PATTERN	"WDTP"

#define MCA_NUM_GPIO_WDG		4
#define MCA_GPIO_WDG_MIN_TOUT_SEC	1
#define MCA_GPIO_WDG_MAX_TOUT_SEC	255

struct mca_wdt_soft {
	bool enabled;
	bool full_reset;
	bool no_way_out;
};

typedef enum mca_wdg_gpio_mode {
	MCA_GPIO_WD_TOGGLE,
	MCA_GPIO_WD_LEVEL_HIGH,
	MCA_GPIO_WD_LEVEL_LOW,
} mca_wdg_gpio_mode_t;

struct mca_wdt_gpio {
	bool full_reset;
	bool no_way_out;
	u32  mca_io_number;
	u32  timeout_sec;
	mca_wdg_gpio_mode_t mode;
};

struct mca_wdt_priv {
	struct mca_wdt_soft wdt_soft;
	u8 wdt_gpio_num;
	struct mca_wdt_gpio wdt_gpio[MCA_NUM_GPIO_WDG];
};

static int mca_wdt_reset(struct udevice *dev)
{
	unsigned char *pattern = (unsigned char *)WDT_REFRESH_PATTERN;
	int ret = 0;

	ret = mca_bulk_write(MCA_WDT_REFRESH_0, pattern, WDT_REFRESH_LEN);

	return ret;
}

static int mca_wdt_stop(struct udevice *dev)
{
	struct mca_wdt_priv *priv = dev_get_priv(dev);
	int ret = 0;
	u8 i;

	ret = mca_update_bits(MCA_WDT_CONTROL, MCA_WDT_ENABLE, 0);
	if (ret)
		printf("Couldn't stop watchdog (%d)\n", ret);

	for (i = 0 ; i < priv->wdt_gpio_num ; i++) {
		ret = mca_update_bits(MCA_GPIO_WDT0_CONTROL + 0x10*i,
				      MCA_WDT_ENABLE, 0);
		if (ret)
			printf("Couldn't stop gpio wdt %d (%d)\n", i, ret);
	}

	return ret;
}

static int mca_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct mca_wdt_priv *priv = dev_get_priv(dev);
	u8 control;
	int ret = 0;
	u8 i;

	if (priv->wdt_soft.enabled) {
		const u16 mca_timeout = timeout / 1000;

		/* Set timeout */
		ret = mca_write_reg(MCA_WDT_TIMEOUT, mca_timeout);
		if (ret) {
			printf("Couldn't set watchdog timeout (%d)\n", ret);
			goto err;
		}

		control = MCA_WDT_ENABLE;
		control |= priv->wdt_soft.no_way_out ? MCA_WDT_NOWAYOUT : 0;
		control |= priv->wdt_soft.full_reset ? MCA_WDT_FULLRESET : 0;

		ret = mca_update_bits(MCA_WDT_CONTROL,
				      MCA_WDT_NOWAYOUT  | MCA_WDT_FULLRESET | MCA_WDT_ENABLE,
				      control);
		if (ret) {
			printf("Couldn't configure watchdog (%d)\n", ret);
			goto err;
		}

		mca_wdt_reset(dev);
	}

	for (i = 0 ; i < priv->wdt_gpio_num ; i++) {
		u8 control;

		ret = mca_write_reg(MCA_GPIO_WDT0_IO + 0x10*i,
				    priv->wdt_gpio[i].mca_io_number);
		if (ret) {
			printf("Couldn't set gpio wdt %d io (%d)\n", i, ret);
			goto err;
		}

		ret = mca_write_reg(MCA_GPIO_WDT0_TIMEOUT + 0x10*i,
				    priv->wdt_gpio[i].timeout_sec);
		if (ret) {
			printf("Couldn't set gpio wdt %d timeout (%d)\n", i, ret);
			goto err;
		}

		control = MCA_WDT_ENABLE;
		control |= priv->wdt_gpio[i].no_way_out ? MCA_WDT_NOWAYOUT : 0;
		control |= priv->wdt_gpio[i].full_reset ? MCA_WDT_FULLRESET : 0;
		control |= priv->wdt_gpio[i].mode << MCA_GPIO_WDT_MODE_SHIFT;

		ret = mca_update_bits(MCA_GPIO_WDT0_CONTROL + 0x10*i,
				      MCA_WDT_NOWAYOUT  | MCA_WDT_FULLRESET | MCA_WDT_ENABLE |
				      MCA_GPIO_WDT_MODE_MASK,
				      control);
		if (ret) {
			printf("Couldn't configure gpio wdt %d (%d)\n", i, ret);
			goto err;
		}
	}
err:
	return ret;
}

static int mca_wdt_probe(struct udevice *dev)
{
	struct mca_wdt_priv *priv = dev_get_priv(dev);
	ofnode node;
	ofnode soft_rfsh, gpio_rfsh;
	const char *mode;
	int ret;
	u8 i = 0;

	/* Parse standard soft-rfsh watchdog device tree entries */
	soft_rfsh = dev_read_subnode(dev, "soft-rfsh");
	/* Make sure soft_rfsh node was found */
	if (ofnode_valid(soft_rfsh)) {
		priv->wdt_soft.enabled = true;
		priv->wdt_soft.full_reset = ofnode_read_bool(soft_rfsh, "digi,full-reset");
		priv->wdt_soft.no_way_out = ofnode_read_bool(soft_rfsh, "digi,no-way-out");
	}

	/* Parse gpio-rfsh watchdog device tree entries */
	gpio_rfsh = dev_read_subnode(dev, "gpio-rfsh");
	/* Make sure gpio_rfsh node was found */
	if (!ofnode_valid(gpio_rfsh))
		goto no_gpio_rfsh;
	ofnode_for_each_subnode(node, gpio_rfsh) {
		if (i == MCA_NUM_GPIO_WDG) {
			printf("%s: Exceeds max num of mca_wdt_gpio nodes (%d)\n",
				__func__, MCA_NUM_GPIO_WDG);
			return -ERANGE;
		}

		priv->wdt_gpio[i].full_reset = ofnode_read_bool(node,
								"digi,full-reset");
		priv->wdt_gpio[i].no_way_out = ofnode_read_bool(node,
								"digi,no-way-out");
		ret = ofnode_read_u32(node, "digi,mca-io-number",
				      &priv->wdt_gpio[i].mca_io_number);
		if (ret) {
			printf("%s: node %s has no mca-io-number\n",
			       __func__, ofnode_get_name(node));
			return -EINVAL;
		}

		ret = ofnode_read_u32(node, "digi,timeout-sec",
				      &priv->wdt_gpio[i].timeout_sec);
		if (ret) {
			printf("%s: node %s has no digi,timeout-sec\n",
			       __func__, ofnode_get_name(node));
			return -EINVAL;
		}
		if (priv->wdt_gpio[i].timeout_sec < MCA_GPIO_WDG_MIN_TOUT_SEC ||
		    priv->wdt_gpio[i].timeout_sec > MCA_GPIO_WDG_MAX_TOUT_SEC) {
			printf("%s: node %s timeout-sec out of range. Set to %d\n",
			       __func__, ofnode_get_name(node),
			       MCA_GPIO_WDG_MAX_TOUT_SEC);
			priv->wdt_gpio[i].timeout_sec = MCA_GPIO_WDG_MAX_TOUT_SEC;
		}

		mode = ofnode_read_string(node, "digi,mode");
		if (!mode) {
			printf("%s: node %s has no mode\n",
			       __func__, ofnode_get_name(node));
			return -EINVAL;
		}

		if (!strcmp(mode, "toggle")) {
			priv->wdt_gpio[i].mode = MCA_GPIO_WD_TOGGLE;
		} else if (!strcmp(mode, "level-high")) {
			priv->wdt_gpio[i].mode = MCA_GPIO_WD_LEVEL_HIGH;
		} else if (!strcmp(mode, "level-low")) {
			priv->wdt_gpio[i].mode = MCA_GPIO_WD_LEVEL_LOW;
		} else {
			printf("%s: Invalid mode '%s'\n", __func__, mode);
			return -EINVAL;
		}

		i++;
	}
no_gpio_rfsh:
	priv->wdt_gpio_num = i;

	if (!priv->wdt_soft.enabled && !priv->wdt_gpio_num)
		return -ENODEV;

	return 0;
}

static const struct wdt_ops mca_wdt_ops = {
	.start		= mca_wdt_start,
	.stop		= mca_wdt_stop,
	.reset		= mca_wdt_reset,
};

static const struct udevice_id mca_wdt_ids[] = {
	{ .compatible = "digi,mca-wdt" },
	{}
};

U_BOOT_DRIVER(mca_wdt) = {
	.name		= "mca_wdt",
	.id		= UCLASS_WDT,
	.of_match	= mca_wdt_ids,
	.probe		= mca_wdt_probe,
	.ops		= &mca_wdt_ops,
	.priv_auto_alloc_size = sizeof(struct mca_wdt_priv),
	.flags		= DM_FLAG_PRE_RELOC,
};
