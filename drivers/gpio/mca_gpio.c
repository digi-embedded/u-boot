/*
 *  Copyright 2021 Digi International Inc
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <common.h>
#include <errno.h>
#include <dm.h>
#include <malloc.h>
#include <asm/gpio.h>
#include <errno.h>
#include "../../board/digi/common/mca_registers.h"
#include "../../board/digi/common/mca.h"

#define GPIO_DIR_REG(x)		(MCA_GPIO_DIR_0 + ((x) / 8))
#define GPIO_DATA_REG(x)	(MCA_GPIO_DATA_0 + ((x) / 8))
#define GPIO_SET_REG(x)		(MCA_GPIO_SET_0 + ((x) / 8))
#define GPIO_CLEAR_REG(x)	(MCA_GPIO_CLEAR_0 + ((x) / 8))
#define BIT_OFFSET(i)		((i) % 8)

#if CONFIG_IS_ENABLED(DM_GPIO)
static int mca_gpio_is_output(int offset)
{
	unsigned char val;

	mca_read_reg(GPIO_DIR_REG(offset), &val);

	return val & (1 << BIT_OFFSET(offset)) ? 1 : 0;
}

/* set GPIO pin 'offset' as an input */
static int mca_gpio_direction_input(struct udevice *dev, unsigned offset)
{
	/* Configure GPIO direction as input. */
	mca_update_bits(GPIO_DIR_REG(offset), 1 << BIT_OFFSET(offset), 0);

	return 0;
}

/* set GPIO pin 'offset' as an output, with polarity 'value' */
static int mca_gpio_direction_output(struct udevice *dev, unsigned offset,
				       int value)
{
	unsigned int reg = value ? GPIO_SET_REG(offset) : GPIO_CLEAR_REG(offset);

	/* Configure GPIO output value. */
	mca_write_reg(reg, 1 << BIT_OFFSET(offset));

	/* Configure GPIO direction as output. */
	mca_update_bits(GPIO_DIR_REG(offset), 1 << BIT_OFFSET(offset),
			1 << BIT_OFFSET(offset));

	return 0;
}

/* read GPIO IN value of pin 'offset' */
static int mca_gpio_get_value(struct udevice *dev, unsigned offset)
{
	unsigned char val;
	int ret;

	ret = mca_read_reg(GPIO_DATA_REG(offset), &val);
	if (ret < 0)
		return ret;

	return (val & (1 << BIT_OFFSET(offset)) ? 1 : 0);
}

/* write GPIO OUT value to pin 'offset' */
static int mca_gpio_set_value(struct udevice *dev, unsigned offset,
				 int value)
{
	unsigned int reg = value ? GPIO_SET_REG(offset) : GPIO_CLEAR_REG(offset);

	mca_write_reg(reg, 1 << BIT_OFFSET(offset));

	return 0;
}

static int mca_gpio_get_function(struct udevice *dev, unsigned offset)
{
	if (mca_gpio_is_output(offset))
		return GPIOF_OUTPUT;
	else
		return GPIOF_INPUT;
}

static const struct dm_gpio_ops gpio_mca_ops = {
	.direction_input	= mca_gpio_direction_input,
	.direction_output	= mca_gpio_direction_output,
	.get_value		= mca_gpio_get_value,
	.set_value		= mca_gpio_set_value,
	.get_function		= mca_gpio_get_function,
};

static int mca_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	int ret;
	unsigned char gpio_num;
	char name[18], *str;

	ret = mca_read_reg(MCA_GPIO_NUM, &gpio_num);
	if (ret) {
		printf("Cannot read MCA_GPIO_NUM\n");
		return -ENODEV;
	}

	sprintf(name, "MCA-GPIO_");
	str = strdup(name);
	if (!str)
		return -ENOMEM;
	uc_priv->bank_name = str;
	uc_priv->gpio_count = gpio_num;

	return 0;
}

static int mca_gpio_bind(struct udevice *dev)
{
	return 0;
}

static const struct udevice_id mca_gpio_ids[] = {
	{ .compatible = "digi,mca-gpio" },
	{ }
};

U_BOOT_DRIVER(gpio_mca) = {
	.name	= "gpio_mca",
	.id	= UCLASS_GPIO,
	.ops	= &gpio_mca_ops,
	.probe	= mca_gpio_probe,
	.of_match = mca_gpio_ids,
	.bind	= mca_gpio_bind,
};
#endif
