/*
 * Copyright (C) 2018 Digi International, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fuse.h>
#include "hwid.h"

/*
 * HWID is stored in 3 NON-consecutive Fuse Words, being:
 *
 *                           MAC1[31:0] (Bank 0 Word 708)
 *
 *               |    31..27   | 26..20  |          19..0           |
 *               +-------------+---------+--------------------------+
 * HWID0[31:0]:  |   Location  |  GenID  |      Serial number       |
 *               +-------------+---------+--------------------------+
 *
 *                           MAC1[47:32] (Bank 0 Word 709)
 *
 *               |          31..16          |  15..8  | 7..4 | 3..0 |
 *               +--------------------------+---------+------+------+
 * HWID1[15:0]:  |         RESERVED         | Variant |  HV  | Cert |
 *               +--------------------------+---------+------+------+
 *
 *                           MAC2[47:32] (Bank 0 Word 711)
 *
 *               |          31..16          | 15..10  | 9..4 | 3..0 |
 *               +--------------------------+---------+------+------+
 * HWID1[31:16]: |         RESERVED         |  Year   | Week |  WID |
 *               +--------------------------+---------+------+------+
 */
int board_read_hwid(u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 fuseword;
	int ret;

	/* HWID0 */
	ret = fuse_read(bank, HWID0_FUSE0, &fuseword);
	if (ret)
		return ret;

	hwid_array[0] = fuseword;

	/* HWID1 */
	ret = fuse_read(bank, HWID1_FUSE1, &fuseword);
	if (ret)
		return ret;

	hwid_array[1] = fuseword & 0xFFFF;

	ret = fuse_read(bank, HWID1_FUSE2, &fuseword);
	if (ret)
		return ret;

	hwid_array[1] |= (fuseword << 16) & 0xFFFF0000;

	return 0;
}

int board_sense_hwid(u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 fuseword;
	int ret;

	/* HWID0 */
	ret = fuse_sense(bank, HWID0_FUSE0, &fuseword);
	if (ret)
		return ret;

	hwid_array[0] = fuseword;

	/* HWID1 */
	ret = fuse_sense(bank, HWID1_FUSE1, &fuseword);
	if (ret)
		return ret;

	hwid_array[1] = fuseword & 0xFFFF;

	ret = fuse_sense(bank, HWID1_FUSE2, &fuseword);
	if (ret)
		return ret;

	hwid_array[1] |= (fuseword << 16) & 0xFFFF0000;

	return 0;
}

int board_prog_hwid(const u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 fuseword;
	int ret;

	/* HWID0 */
	fuseword = hwid_array[0];
	ret = fuse_prog(bank, HWID0_FUSE0, fuseword);
	if (ret)
		return ret;

	/* HWID1 */
	fuseword = hwid_array[1] & 0xFFFF;
	ret = fuse_prog(bank, HWID1_FUSE1, fuseword);
	if (ret)
		return ret;

	fuseword = (hwid_array[1] >> 16) & 0xFFFF;
	ret = fuse_prog(bank, HWID1_FUSE2, fuseword);
	if (ret)
		return ret;

	return 0;
}

int board_override_hwid(const u32 *hwid_array)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 fuseword;
	int ret;

	/* HWID0 */
	fuseword = hwid_array[0];
	ret = fuse_override(bank, HWID0_FUSE0, fuseword);
	if (ret)
		return ret;

	/* HWID1 */
	fuseword = hwid_array[1] & 0xFFFF;
	ret = fuse_override(bank, HWID1_FUSE1, fuseword);
	if (ret)
		return ret;

	fuseword = (hwid_array[1] >> 16) & 0xFFFF;
	ret = fuse_override(bank, HWID1_FUSE2, fuseword);
	if (ret)
		return ret;

	return 0;
}

int board_lock_hwid(void)
{
	/* SCU performs automatic lock after programming */
	return 0;
}
