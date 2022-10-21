/*
 * (C) Copyright 2018 Digi International, Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <command.h>
#include <common.h>
#include <linux/errno.h>
#include <fuse.h>
#include "hwid.h"

extern int hwid_word_lengths[CONFIG_HWID_WORDS_NUMBER];
typedef struct mac_base { uint8_t mbase[3]; } mac_base_t;

mac_base_t mac_pools[] = {
	[1] = {{0x00, 0x04, 0xf3}},
	[2] = {{0x00, 0x40, 0x9d}},
};

__weak int board_read_hwid(struct digi_hwid *hwid)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	u32 fuseword;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_read(bank, word, &fuseword);
		((u32 *)hwid)[i] = fuseword;
		if (ret)
			return ret;
	}

	return 0;
}

__weak int board_sense_hwid(struct digi_hwid *hwid)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	u32 fuseword;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		ret = fuse_sense(bank, word, &fuseword);
		((u32 *)hwid)[i] = fuseword;
		if (ret)
			return ret;
	}

	return 0;
}

__weak void board_update_hwid(bool is_fuse)
{
	/* Do nothing */
}

__weak int board_prog_hwid(const struct digi_hwid *hwid)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	u32 fuseword;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		fuseword = ((u32 *)hwid)[i];
		ret = fuse_prog(bank, word, fuseword);
		if (ret)
			return ret;
	}

	/* Trigger a HWID-related variables update (from fuses)*/
	board_update_hwid(true);
	return 0;
}

__weak int board_override_hwid(const struct digi_hwid *hwid)
{
	u32 bank = CONFIG_HWID_BANK;
	u32 word = CONFIG_HWID_START_WORD;
	u32 cnt = CONFIG_HWID_WORDS_NUMBER;
	u32 fuseword;
	int ret, i;

	for (i = 0; i < cnt; i++, word++) {
		fuseword = ((u32 *)hwid)[i];
		ret = fuse_override(bank, word, fuseword);
		if (ret)
			return ret;
	}

	/* Trigger a HWID-related variables update (from shadow registers)*/
	board_update_hwid(false);
	return 0;
}

#ifndef CONFIG_CC8X
__weak int board_lock_hwid(void)
{
	return fuse_prog(OCOTP_LOCK_BANK, OCOTP_LOCK_WORD,
			CONFIG_HWID_LOCK_FUSE);
}
#endif

__weak void print_hwid_hex(struct digi_hwid *hwid)
{
	int i;

	for (i = CONFIG_HWID_WORDS_NUMBER - 1; i >= 0; i--)
		printf(" %.*x", hwid_word_lengths[i], ((u32 *)hwid)[i]);

	printf("\n");
}

__weak bool board_has_eth1(void)
{
	return false;
}

__weak bool board_has_wireless(void)
{
	/* assume it has, by default */
	return true;
}

__weak bool board_has_bluetooth(void)
{
	/* assume it has, by default */
	return true;
}

static int set_lower_mac(uint32_t val, uint8_t *mac)
{
	mac[3] = (uint8_t)(val >> 16);
	mac[4] = (uint8_t)(val >> 8);
	mac[5] = (uint8_t)(val);

	return 0;
}

void hwid_get_macs(uint32_t pool, uint32_t base)
{
	uint8_t macaddr[6];
	char macvars[4][10];
	int ret, n_macs = 0;
	char cmd[CONFIG_SYS_CBSIZE] = "";

	/*
	 * Setting the mac pool to 0 means that the mac addresses will not be
	 * setup with the information encoded in the efuses.
	 * This is a back-door to allow manufacturing units with uboots that
	 * do not support some specific pool.
	 */
	if (pool == 0)
		return;

	if (pool > ARRAY_SIZE(mac_pools)) {
		printf("ERROR: unsupported MAC address pool %u\n", pool);
		return;
	}

	/* Set MAC from pool */
	memcpy(macaddr, mac_pools[pool].mbase, sizeof(mac_base_t));

	/* Fill in env-variables array, depending on available NICs */
	strcpy(macvars[n_macs], "ethaddr");
	n_macs++;

	if (board_has_eth1()) {
		strcpy(macvars[n_macs], "eth1addr");
		n_macs++;
	}

	if (board_has_wireless()) {
		strcpy(macvars[n_macs], "wlanaddr");
		n_macs++;
	}

	if (board_has_bluetooth()) {
		strcpy(macvars[n_macs], "btaddr");
		n_macs++;
	}

	/* Protect from overflow */
	if (base + n_macs > 0xffffff) {
		printf("ERROR: not enough remaining MACs on this MAC pool\n");
		return;
	}

	for (int i = 0; i < n_macs; i++) {
		set_lower_mac(base + i, macaddr);

		sprintf(cmd, "setenv -f %s %pM", macvars[i], macaddr);
		ret = run_command(cmd, 0);
		if (ret)
			printf("ERROR setting %s from fuses (%d)\n", macvars[i],
			       ret);
	}
}
