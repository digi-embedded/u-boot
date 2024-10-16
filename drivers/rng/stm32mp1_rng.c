// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2019, Linaro Limited
 */

#define LOG_CATEGORY UCLASS_RNG

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <log.h>
#include <reset.h>
#include <rng.h>
#include <linux/bitops.h>
#include <linux/delay.h>

#include <asm/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>

#define RNG_CR			0x00
#define RNG_CR_RNGEN		BIT(2)
#define RNG_CR_CED		BIT(5)
#define RNG_CR_CLKDIV_SHIFT	16
#define RNG_CR_CONDRST		BIT(30)

#define RNG_SR		0x04
#define RNG_SR_SEIS	BIT(6)
#define RNG_SR_CEIS	BIT(5)
#define RNG_SR_SECS	BIT(2)
#define RNG_SR_DRDY	BIT(0)

#define RNG_DR		0x08

/*
 * struct stm32_rng_data - RNG compat data
 *
 * @max_clock_rate:	Max RNG clock frequency, in Hertz
 * @has_cond_reset:	True if conditionnal reset is supported
 *
 */
struct stm32_rng_data {
	uint max_clock_rate;
	bool has_cond_reset;
};

struct stm32_rng_plat {
	fdt_addr_t base;
	struct clk clk;
	struct reset_ctl rst;
	const struct stm32_rng_data *data;
};

static int stm32_rng_read(struct udevice *dev, void *data, size_t len)
{
	int retval, i;
	u32 sr, count, reg;
	size_t increment;
	struct stm32_rng_plat *pdata = dev_get_plat(dev);

	while (len > 0) {
		retval = readl_poll_timeout(pdata->base + RNG_SR, sr,
					    sr & RNG_SR_DRDY, 10000);
		if (retval)
			return retval;

		if (sr & (RNG_SR_SEIS | RNG_SR_SECS)) {
			/* As per SoC TRM */
			clrbits_le32(pdata->base + RNG_SR, RNG_SR_SEIS);
			for (i = 0; i < 12; i++)
				readl(pdata->base + RNG_DR);
			if (readl(pdata->base + RNG_SR) & RNG_SR_SEIS) {
				log_err("RNG Noise");
				return -EIO;
			}
			/* start again */
			continue;
		}

		/*
		 * Once the DRDY bit is set, the RNG_DR register can
		 * be read four consecutive times.
		 */
		count = 4;
		while (len && count) {
			reg = readl(pdata->base + RNG_DR);
			memcpy(data, &reg, min(len, sizeof(u32)));
			increment = min(len, sizeof(u32));
			data += increment;
			len -= increment;
			count--;
		}
	}

	return 0;
}

static uint stm32_rng_clock_freq_restrain(struct stm32_rng_plat *pdata)
{
	ulong clock_rate = 0;
	uint clock_div = 0;

	clock_rate = clk_get_rate(&pdata->clk);

	/*
	 * Get the exponent to apply on the CLKDIV field in RNG_CR register.
	 * No need to handle the case when clock-div > 0xF as it is physically
	 * impossible.
	 */
	while ((clock_rate >> clock_div) > pdata->data->max_clock_rate)
		clock_div++;

	log_debug("RNG clk rate : %lu\n", clk_get_rate(&pdata->clk) >> clock_div);

	return clock_div;
}

static int stm32_rng_init(struct stm32_rng_plat *pdata)
{
	int err;
	u32 cr, sr;

	err = clk_enable(&pdata->clk);
	if (err)
		return err;

	cr = readl(pdata->base + RNG_CR);

	/* Disable CED */
	cr |= RNG_CR_CED;
	if (pdata->data->has_cond_reset) {
		uint clock_div = stm32_rng_clock_freq_restrain(pdata);

		cr |= RNG_CR_CONDRST | (clock_div << RNG_CR_CLKDIV_SHIFT);
		writel(cr, pdata->base + RNG_CR);
		cr &= ~RNG_CR_CONDRST;
		writel(cr, pdata->base + RNG_CR);
		err = readl_poll_timeout(pdata->base + RNG_CR, cr,
					 (!(cr & RNG_CR_CONDRST)), 10000);
		if (err)
			return err;
	}

	/* clear error indicators */
	writel(0, pdata->base + RNG_SR);

	cr |= RNG_CR_RNGEN;
	writel(cr, pdata->base + RNG_CR);

	err = readl_poll_timeout(pdata->base + RNG_SR, sr,
				 sr & RNG_SR_DRDY, 10000);
	return err;
}

static int stm32_rng_cleanup(struct stm32_rng_plat *pdata)
{
	writel(0, pdata->base + RNG_CR);

	return clk_disable(&pdata->clk);
}

static int stm32_rng_probe(struct udevice *dev)
{
	struct stm32_rng_plat *pdata = dev_get_plat(dev);

	pdata->data = (struct stm32_rng_data *)dev_get_driver_data(dev);

	reset_assert(&pdata->rst);
	udelay(20);
	reset_deassert(&pdata->rst);

	return stm32_rng_init(pdata);
}

static int stm32_rng_remove(struct udevice *dev)
{
	struct stm32_rng_plat *pdata = dev_get_plat(dev);

	return stm32_rng_cleanup(pdata);
}

static int stm32_rng_of_to_plat(struct udevice *dev)
{
	struct stm32_rng_plat *pdata = dev_get_plat(dev);
	int err;

	pdata->base = dev_read_addr(dev);
	if (!pdata->base)
		return -ENOMEM;

	err = clk_get_by_index(dev, 0, &pdata->clk);
	if (err)
		return err;

	err = reset_get_by_index(dev, 0, &pdata->rst);
	if (err)
		return err;

	return 0;
}

static const struct dm_rng_ops stm32_rng_ops = {
	.read = stm32_rng_read,
};

static const struct stm32_rng_data stm32mp13_rng_data = {
	.has_cond_reset = true,
	.max_clock_rate = 48000000,
};

static const struct stm32_rng_data stm32_rng_data = {
	.has_cond_reset = false,
	.max_clock_rate = 48000000,
};

static const struct udevice_id stm32_rng_match[] = {
	{.compatible = "st,stm32mp13-rng", .data = (ulong)&stm32mp13_rng_data},
	{.compatible = "st,stm32-rng", .data = (ulong)&stm32_rng_data},
	{},
};

U_BOOT_DRIVER(stm32_rng) = {
	.name = "stm32-rng",
	.id = UCLASS_RNG,
	.of_match = stm32_rng_match,
	.ops = &stm32_rng_ops,
	.probe = stm32_rng_probe,
	.remove = stm32_rng_remove,
	.plat_auto	= sizeof(struct stm32_rng_plat),
	.of_to_plat = stm32_rng_of_to_plat,
};
