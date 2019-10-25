/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <dm/uclass.h>
#include <common.h>
#include <i2c.h>
#include "hwid.h"
#include "mca_registers.h"

extern struct digi_hwid my_hwid;

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
			retries--;
			continue;
		}

		ret = mca_write_reg(MCA_CTRL_0, MCA_CTRL_0_RESET);
		if (ret) {
			retries--;
			continue;
		}

		break;
	} while (retries);

	if (!retries)
		printf("MCA: unable to perform a fw reset (%d)\n", ret);
}

void mca_save_cfg(void)
{
	int ret;
	int retries = 3;
	uint8_t unlock_data[] = {'C', 'T', 'R', 'U'};

	do {
		/* First, unlock the CTRL_0 register access */
		ret = mca_bulk_write(MCA_CTRL_UNLOCK_0, unlock_data,
				     sizeof(unlock_data));
		if (ret) {
			retries--;
			continue;
		}

		ret = mca_update_bits(MCA_CTRL_0, MCA_CTRL_0_SAVE_CFG, MCA_CTRL_0_SAVE_CFG);
		if (ret) {
			retries--;
			continue;
		}

		break;
	} while (retries);

	if (!retries)
		printf("MCA: unable to save configuration (%d)\n", ret);
}

void mca_somver_update(void)
{
	unsigned char somver;
	int ret;

	/*
	 * Read the som version stored in MCA.
	 * If it doesn't match with real SOM version read from my_hwid.hv:
	 *    - update it into the MCA.
	 *    - force the new value to be saved in MCA NVRAM.
	 * The purpose of this functionality is that MCA starts using the
	 * correct SOM version since boot.
	 */
	ret = mca_read_reg(MCA_HWVER_SOM, &somver);
	if (ret) {
		printf("Cannot read MCA_HWVER_SOM\n");
	} else {
		if (my_hwid.hv != somver) {
			ret = mca_write_reg(MCA_HWVER_SOM, my_hwid.hv);
			if (ret)
				printf("Cannot write MCA_HWVER_SOM\n");
			else
				mca_save_cfg();
		}
	}
}

void mca_init(void)
{
	unsigned char devid = 0;
	unsigned char hwver;
	unsigned char fwver[2];
	int ret, fwver_ret;

#ifdef CONFIG_DM_I2C
	struct udevice *bus, *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, CONFIG_MCA_I2C_BUS, &bus);
	if (ret) {
		printf("ERROR: getting %d bus for MCA\n", CONFIG_MCA_I2C_BUS);
		return;
	}

	ret = dm_i2c_probe(bus, CONFIG_MCA_I2C_ADDR, 0, &dev);
	if (ret) {
		printf("ERROR: can't find MCA at address %x\n",
		       CONFIG_MCA_I2C_ADDR);
		return;
	}

	ret = i2c_set_chip_offset_len(dev, 2);
	if (ret)
		printf("ERROR: setting address len to 2 for MCA\n");
#endif
	ret = mca_read_reg(MCA_DEVICE_ID, &devid);
	if (devid != BOARD_MCA_DEVICE_ID) {
		printf("MCA: invalid MCA DEVICE ID (0x%02x)\n", devid);
		return;
	}

	ret = mca_read_reg(MCA_HW_VER, &hwver);
	fwver_ret = mca_bulk_read(MCA_FW_VER_L, fwver, 2);

	printf("MCA:   HW_VER=");
	if (ret)
		printf("??");
	else
		printf("%d", hwver);

	printf("  FW_VER=");
	if (fwver_ret)
		printf("??");
	else
		printf("%d.%02d %s", fwver[1] & 0x7f, fwver[0],
		       fwver[1] & 0x80 ? "(alpha)" : "");
	printf("\n");
}
