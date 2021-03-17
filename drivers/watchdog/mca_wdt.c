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

struct mca_wdt_priv {
	bool full_reset;
	bool no_way_out;
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
	int ret = 0;

	ret = mca_update_bits(MCA_WDT_CONTROL, MCA_WDT_ENABLE, 0);
	if (ret)
		printf("Could not stop watchdog (%d)\n", ret);

	return ret;
}

static int mca_wdt_start(struct udevice *dev, u64 timeout, ulong flags)
{
	struct mca_wdt_priv *priv = dev_get_priv(dev);
	u16 mca_timeout;
	u8 control;
	int ret = 0;

	mca_timeout = timeout / 1000;

	/* Set timeout */
	ret = mca_write_reg(MCA_WDT_TIMEOUT, mca_timeout);
	if (ret) {
		printf("Could not set watchdog timeout (%d)\n", ret);
		goto err;
	}

	control = MCA_WDT_ENABLE;
	control |= priv->no_way_out ? MCA_WDT_NOWAYOUT : 0;
	control |= priv->full_reset ? MCA_WDT_FULLRESET : 0;

	ret = mca_update_bits(MCA_WDT_CONTROL,
			      MCA_WDT_NOWAYOUT  | MCA_WDT_IRQNORESET |
			      MCA_WDT_FULLRESET | MCA_WDT_ENABLE,
			      control);
	if (ret) {
		printf("Could not configure watchdog (%d)\n", ret);
		goto err;
	}

	mca_wdt_reset(dev);
err:
	return ret;
}

static int mca_wdt_probe(struct udevice *dev)
{
	struct mca_wdt_priv *priv = dev_get_priv(dev);

	priv->full_reset = dev_read_bool(dev, "digi,full-reset");
	priv->no_way_out = dev_read_bool(dev, "digi,no-way-out");

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
