/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <i2c.h>
#include "mca_registers.h"

int mca_read_reg(int reg, unsigned char *value)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_MCA_I2C_BUS))
		return -1;
#endif

	if (i2c_probe(CONFIG_MCA_I2C_ADDR)) {
		printf("ERR: cannot access the MCA\n");
		return -1;
	}

	if (i2c_read(CONFIG_MCA_I2C_ADDR, reg, 2, value, 1))
		return -1;

	return 0;
}

int mca_bulk_read(int reg, unsigned char *values, int len)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_MCA_I2C_BUS))
		return -1;
#endif

	if (i2c_probe(CONFIG_MCA_I2C_ADDR)) {
		printf("ERR: cannot access the MCA\n");
		return -1;
	}

	if (i2c_read(CONFIG_MCA_I2C_ADDR, reg, 2, values, len))
		return -1;

	return 0;
}

int mca_write_reg(int reg, unsigned char value)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_MCA_I2C_BUS))
		return -1;
#endif

	if (i2c_probe(CONFIG_MCA_I2C_ADDR)) {
		printf("ERR: cannot access the MCA\n");
		return -1;
	}

	if (i2c_write(CONFIG_MCA_I2C_ADDR, reg, 2, &value, 1))
		return -1;

	return 0;
}

int mca_bulk_write(int reg, unsigned char *values, int len)
{
#ifdef CONFIG_I2C_MULTI_BUS
	if (i2c_set_bus_num(CONFIG_MCA_I2C_BUS))
		return -1;
#endif

	if (i2c_probe(CONFIG_MCA_I2C_ADDR)) {
		printf("ERR: cannot access the MCA\n");
		return -1;
	}

	if (i2c_write(CONFIG_MCA_I2C_ADDR, reg, 2, values, len))
		return -1;

	return 0;
}

int mca_update_bits(int reg, unsigned char mask, unsigned char val)
{
	int ret;
	unsigned char tmp, orig;

	ret = mca_read_reg(reg, &orig);
	if (ret != 0)
		return ret;

	tmp = orig & ~mask;
	tmp |= val & mask;

	if (tmp != orig)
		return mca_write_reg(reg, tmp);

	return 0;
}

void mca_reset(void)
{
	int ret;
	int retries = 3;
	uint8_t unlock_data[] = {'C', 'T', 'R', 'U'};

	do {
		/* First, unlock the CTRL_0 register access */
		ret = mca_bulk_write(MCA_CTRL_UNLOCK_0, unlock_data,
				     sizeof(unlock_data));
		if (ret) {
			printf("MCA: unable to unlock CTRL register (%d)\n", ret);
			retries--;
			continue;
		}

		ret = mca_write_reg(MCA_CTRL_0, MCA_CTRL_0_RESET);
		if (ret) {
			printf("MCA: unable to perform a fw reset (%d)\n", ret);
			retries--;
			continue;
		}

		break;
	} while (retries);
}
