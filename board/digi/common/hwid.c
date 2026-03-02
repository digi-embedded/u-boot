/*
 * (C) Copyright 2018-2026 Digi International, Inc.
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
#include "../common/helper.h"

extern struct digi_hwid_fuse hwid_fuse_map[];
extern unsigned int hwid_nwords;
typedef struct mac_base { uint8_t mbase[3]; } mac_base_t;

mac_base_t mac_pools[] = {
	[1] = {{0x00, 0x04, 0xf3}},
	[2] = {{0x00, 0x40, 0x9d}},
};

__weak int board_read_hwid(struct digi_hwid *hwid)
{
	u32 fuseword;
	int ret;

	for (int i = 0; i < hwid_nwords; i++) {
		ret = fuse_sense(hwid_fuse_map[i].bank,
				 hwid_fuse_map[i].word,
				 &fuseword);
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

__weak void board_unlock_fuse_prog()
{
	/* Do nothing */
}

__weak void board_lock_fuse_prog()
{
	/* Do nothing */
}

__weak int board_prog_hwid(const struct digi_hwid *hwid)
{
	u32 fuseword;
	int ret = -1;

	board_unlock_fuse_prog();

	for (int i = 0; i < hwid_nwords; i++) {
		fuseword = ((u32 *)hwid)[i];
		ret = fuse_prog(hwid_fuse_map[i].bank,
				hwid_fuse_map[i].word,
				fuseword);
		if (ret)
			break;
	}

	board_lock_fuse_prog();

	/* Trigger a HWID-related variables update (from FUSE HWID)*/
	if (!ret)
		board_update_hwid(true);

	return ret;
}

__weak int board_lock_hwid(void)
{
#ifdef CONFIG_HAS_OTP_LOCK_FUSE
	return fuse_prog(OCOTP_LOCK_BANK, OCOTP_LOCK_WORD,
			 CONFIG_HWID_LOCK_FUSE);
#else
	return -1;
#endif
}

int hwid_env_read(struct digi_hwid *hwid)
{
	char var[20];

	/* Get HWID from hwid_n variables */
	for (int i = 0; i < hwid_nwords; i++) {
		sprintf(var, "hwid_%d", i);
		if (env_get(var) == NULL)
			return -1;

		((u32 *)hwid)[i] = env_get_hex(var, 0);
	}

	return 0;
}

int hwid_env_prog(const struct digi_hwid *hwid)
{
	char cmd[80];
	int ret;

	/* Set hwid_n variables from a given HWID */
	for (int i = 0; i < hwid_nwords; i++) {
		sprintf(cmd, "setenv -f hwid_%d %08x", i, ((u32 *) hwid)[i]);
		ret = run_command(cmd, 0);
		if (ret)
			return -1;
	}

	/* Trigger a HWID-related variables update (from ENV HWID)*/
	board_update_hwid(false);

	return 0;
}

int hwid_env_clear(void)
{
	char cmd[80];
	int ret;

	/* Clear hwid_n variables */
	for (int i = 0; i < hwid_nwords; i++) {
		sprintf(cmd, "setenv -f hwid_%d", i);
		ret = run_command(cmd, 0);
		if (ret)
			return -1;
	}

	/* Trigger a HWID-related variables update (from FUSE HWID)*/
	board_update_hwid(true);

	return 0;
}

__weak void print_hwid_hex(struct digi_hwid *hwid)
{
	for (int i = hwid_nwords - 1; i >= 0; i--)
		printf(" %.*x", hwid_fuse_map[i].len, ((u32 *)hwid)[i]);

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

	if (pool >= ARRAY_SIZE(mac_pools)) {
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

void hwid_get_serial_number(const struct digi_hwid *hwid)
{
	char cmd[CONFIG_SYS_CBSIZE] = "";
	int ret;

	/* If year is not set avoid setting this variable */
	if (hwid->year == 0)
		return;

#if (CONFIG_DIGI_FAMILY_ID != 0)
	int week_month = hwid->week;
#ifdef CONFIG_CC8X
	/* If the week is not defined, print the month */
	if (!hwid->week)
		week_month = hwid->month;
#endif
	/* New format with Family ID*/
	sprintf(cmd, "setenv -f serial# %02d%02d%02d%06d",
		hwid->year,
		week_month,
		CONFIG_DIGI_FAMILY_ID,
		hwid->sn);
#else
	/* Old format with Generator ID + Location */
	sprintf(cmd, "setenv -f serial# %c%02d%02d%02d%06d",
		hwid->location + 'A',
		hwid->year,
		hwid->week,
		hwid->genid,
		hwid->sn);
#endif
	ret = run_command(cmd, 0);
	if (ret)
		printf("ERROR setting 'serial#' from fuses (%d)\n", ret);
}

/* Parse HWID info in HWID format */
__weak int board_parse_hwid(int argc, char *const argv[], struct digi_hwid *hwid)
{
	int word;
	u32 hwidword;

	if (argc != hwid_nwords)
		goto err;

	/* Parse backwards, from MSB to LSB */
	word = hwid_nwords - 1;
	for (int i = 0; i < hwid_nwords; i++, word--)
		if (strlen(argv[i]) > hwid_fuse_map[word].len)
			goto err;

	/*
	 * Digi HWID is set as a number of hex strings in the form
	 *   CC6?: <XXXXXXXX> <YYYYYYYY>
	 *   CC8X: <WWWW> <XXXXXXXX> <YYYY> <ZZZZZZZZ>
	 *   CC8M: <XXXXXXXX> <YYYYYYYY> <ZZZZZZZZ>
	 *   CC9X: <XXXXXXXX> <YYYYYYYY> <ZZZZZZZZ>
	 * that are inversely stored into the structure.
	 */

	/* Parse backwards, from MSB to LSB */
	word = hwid_nwords - 1;
	for (int i = 0; i < hwid_nwords; i++, word--) {
		if (strtou32(argv[i], 16, &hwidword))
			goto err;

		((u32 *)hwid)[word] = hwidword;
	}
	board_print_hwid(hwid);

	return 0;

err:
	printf("Invalid HWID input.\n"
		"HWID input must be in the form: "
		CONFIG_HWID_STRINGS_HELP "\n");
	return -EINVAL;
}
