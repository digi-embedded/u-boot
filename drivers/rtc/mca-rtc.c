/*
 *  Copyright 2021 Digi International Inc
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <rtc.h>
#include <dm/device_compat.h>
#include "../../board/digi/common/mca_registers.h"
#include "../../board/digi/common/mca.h"

#define CLOCK_DATA_LEN	(MCA_RTC_COUNT_SEC - MCA_RTC_COUNT_YEAR_L + 1)

enum {
	DATA_YEAR_L,
	DATA_YEAR_H,
	DATA_MONTH,
	DATA_DAY,
	DATA_HOUR,
	DATA_MIN,
	DATA_SEC,
};

static void mca_data_to_tm(u8 *data, struct rtc_time *tm)
{
	/* conversion from MCA RTC to struct time */
	tm->tm_year = (((data[DATA_YEAR_H] &
			 MCA_RTC_YEAR_H_MASK) << 8) |
			 (data[DATA_YEAR_L] & MCA_RTC_YEAR_L_MASK));
	tm->tm_mon  = (data[DATA_MONTH] & MCA_RTC_MONTH_MASK);
	tm->tm_mday = (data[DATA_DAY]   & MCA_RTC_DAY_MASK);
	tm->tm_hour = (data[DATA_HOUR]  & MCA_RTC_HOUR_MASK);
	tm->tm_min  = (data[DATA_MIN]   & MCA_RTC_MIN_MASK);
	tm->tm_sec  = (data[DATA_SEC]   & MCA_RTC_SEC_MASK);

	/* Following fields are not supported in this RTC */
	tm->tm_wday = -1;
	tm->tm_yday  = 0;
	tm->tm_isdst = 0;
}

static void mca_tm_to_data(const struct rtc_time *tm, u8 *data)
{
	data[DATA_YEAR_L]  &= (u8)~MCA_RTC_YEAR_L_MASK;
	data[DATA_YEAR_H]  &= (u8)~MCA_RTC_YEAR_H_MASK;
	data[DATA_YEAR_L]  |= tm->tm_year & MCA_RTC_YEAR_L_MASK;
	data[DATA_YEAR_H]  |= (tm->tm_year >> 8) & MCA_RTC_YEAR_H_MASK;

	data[DATA_MONTH] &= ~MCA_RTC_MONTH_MASK;
	data[DATA_MONTH] |= tm->tm_mon  & MCA_RTC_MONTH_MASK;

	data[DATA_DAY]   &= ~MCA_RTC_DAY_MASK;
	data[DATA_DAY]   |= tm->tm_mday & MCA_RTC_DAY_MASK;

	data[DATA_HOUR]  &= ~MCA_RTC_HOUR_MASK;
	data[DATA_HOUR]  |= tm->tm_hour & MCA_RTC_HOUR_MASK;

	data[DATA_MIN]   &= ~MCA_RTC_MIN_MASK;
	data[DATA_MIN]   |= tm->tm_min & MCA_RTC_MIN_MASK;

	data[DATA_SEC]   &= ~MCA_RTC_SEC_MASK;
	data[DATA_SEC]   |= tm->tm_sec & MCA_RTC_SEC_MASK;
}

static int __mca_rtc_get(struct rtc_time *t)
{
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	int ret;

	ret = mca_bulk_read(MCA_RTC_COUNT_YEAR_L, data, CLOCK_DATA_LEN);
	if (ret < 0) {
		printf("Failed to read RTC time data: %d\n", ret);
		return ret;
	}

	mca_data_to_tm(data, t);
	return 0;
}

static int __mca_rtc_set(const struct rtc_time *t)
{
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	int ret;

	mca_tm_to_data(t, data);

	ret = mca_bulk_write(MCA_RTC_COUNT_YEAR_L, data, CLOCK_DATA_LEN);
	if (ret < 0) {
		printf("Failed to set RTC time data: %d\n", ret);
		return ret;
	}

	return ret;
}

static int __mca_rtc_reset(void)
{
	u8 data[CLOCK_DATA_LEN] = { [0 ... (CLOCK_DATA_LEN - 1)] = 0 };
	unsigned char sec_init, sec_end;
	int ret;

	/* Enable RTC hardware */
	ret = mca_update_bits(MCA_RTC_CONTROL, MCA_RTC_EN, MCA_RTC_EN);
	if (ret < 0) {
		printf("Failed to enable RTC.\n");
		return ret;
	}

	/* no init routine for this RTC needed, just check that it's working */
	ret = mca_bulk_read(MCA_RTC_COUNT_YEAR_L, data, CLOCK_DATA_LEN);
	if (ret < 0) {
		printf("Failed to read RTC sec_init: %d\n", ret);
		return ret;
	}
	sec_init = data[DATA_SEC] & MCA_RTC_SEC_MASK;

	udelay(1000000);

	ret = mca_bulk_read(MCA_RTC_COUNT_YEAR_L, data, CLOCK_DATA_LEN);
	if (ret < 0) {
		printf("Failed to read RTC sec_end: %d\n", ret);
		return ret;
	}
	sec_end = data[DATA_SEC] & MCA_RTC_SEC_MASK;

	if (sec_init == sec_end) {
		printf("Error: RTC didn't increment. sec=%d\n", sec_end);
		return -EIO;
	}

	return 0;
}

#ifndef CONFIG_DM_RTC
int rtc_get(struct rtc_time *t)
{
	return __mca_rtc_get(t);
}

int rtc_set(struct rtc_time *t)
{
	return __mca_rtc_set(t);
}
void rtc_reset(void)
{
	__mca_rtc_reset();
}
#else /* CONFIG_DM_RTC */
static int mca_rtc_get(struct udevice *dev, struct rtc_time *tm)
{
	return __mca_rtc_get(tm);
}

static int mca_rtc_set(struct udevice *dev, const struct rtc_time *tm)
{
	return __mca_rtc_set(tm);
}

static int mca_rtc_reset(struct udevice *dev)
{
	return __mca_rtc_reset();
}

static int mca_rtc_read8(struct udevice *dev, unsigned int reg)
{
	u8 data;
	int ret;

	ret = dm_i2c_read(dev, reg, &data, sizeof(data));
	return ret < 0 ? ret : data;
}

static int mca_rtc_write8(struct udevice *dev, unsigned int reg, int val)
{
	u8 data = val;

	return dm_i2c_write(dev, reg, &data, 1);
}

static const struct rtc_ops mca_rtc_ops = {
	.get = mca_rtc_get,
	.set = mca_rtc_set,
	.read8 = mca_rtc_read8,
	.write8 = mca_rtc_write8,
	.reset = mca_rtc_reset,
};

static const struct udevice_id mca_rtc_ids[] = {
	{ .compatible = "digi,mca-rtc" },
	{ }
};

U_BOOT_DRIVER(rtc_mca) = {
	.name	= "rtc-mca",
	.id	= UCLASS_RTC,
	.of_match = mca_rtc_ids,
	.ops	= &mca_rtc_ops,
};
#endif /* CONFIG_DM_RTC */
