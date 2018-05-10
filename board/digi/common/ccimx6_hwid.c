/*
 * Copyright (C) 2016-2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <fuse.h>
#include <linux/errno.h>
#include "hwid.h"

/*
 * HWID is stored in 2 consecutive Fuse Words, being:
 *
 *                      MAC 0 (Bank 4 Word 2)
 *
 *         |    31..27   | 26..20  |          19..0           |
 *         +-------------+---------+--------------------------+
 * HWID0:  |   Location  |  GenID  |      Serial number       |
 *         +-------------+---------+--------------------------+
 *
 *                      MAC 1 (Bank 4 Word 3)
 *
 *         | 31..26 | 25..20 | 19..16 |  15..8  | 7..4 | 3..0 |
 *         +--------+--------+--------+---------+------+------+
 * HWID1:  |  Year  |  Week  |  WID   | Variant |  HV  | Cert |
 *         +--------+--------+--------+---------+------+------+
 */
int board_read_hwid(u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = HWID_ARRAY_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_read(bank, word, &hwid_array[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_sense_hwid(u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = HWID_ARRAY_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_sense(bank, word, &hwid_array[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_prog_hwid(const u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = HWID_ARRAY_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_prog(bank, word, hwid_array[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_override_hwid(const u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = HWID_ARRAY_WORDS_NUMBER;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_override(bank, word, hwid_array[i]);
		if (ret)
			return ret;
	}

	return 0;
}

int board_lock_hwid(void)
{
	return fuse_prog(OCOTP_LOCK_BANK, OCOTP_LOCK_WORD,
			CONFIG_HWID_LOCK_FUSE);
}
