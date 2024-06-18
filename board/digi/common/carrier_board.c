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

#include <common.h>
#include <env.h>
#include <fdt_support.h>
#include <fuse.h>
#include <configs/digi_common.h>
#include "carrier_board.h"
#include "helper.h"

#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
extern unsigned int board_version;

#if defined(CONFIG_CARRIERBOARD_VERSION_ON_OTP)
unsigned int get_carrierboard_version(void)
{
	u32 version;

	if (fuse_read(CONFIG_CARRIERBOARD_VERSION_BANK,
		      CONFIG_CARRIERBOARD_VERSION_WORD, &version))
		return CARRIERBOARD_VERSION_UNDEFINED;

	version >>= CONFIG_CARRIERBOARD_VERSION_OFFSET;
	version &= CONFIG_CARRIERBOARD_VERSION_MASK;

	return (int)version;
}
#else /* carrier board version on ENV */
unsigned int get_carrierboard_version(void)
{
	char const *str_version;
	u32 version;

	str_version = env_get("board_version");

	if (str_version != NULL) {
		strtou32(str_version, 10, &version);
		return version;
	}

	return CARRIERBOARD_VERSION_UNDEFINED;
}
#endif /* CONFIG_CARRIERBOARD_VERSION_ON_OTP */
#else
unsigned int get_carrierboard_version(void)
{
	return CARRIERBOARD_VERSION_UNDEFINED;
}
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */

#ifdef CONFIG_HAS_CARRIERBOARD_ID
extern unsigned int board_id;

#ifdef CONFIG_CARRIERBOARD_ID_ON_OTP
unsigned int get_carrierboard_id(void)
{
	u32 id;

	if (fuse_read(CONFIG_CARRIERBOARD_ID_BANK,
		      CONFIG_CARRIERBOARD_ID_WORD, &id))
		return CARRIERBOARD_ID_UNDEFINED;

	id >>= CONFIG_CARRIERBOARD_ID_OFFSET;
	id &= CONFIG_CARRIERBOARD_ID_MASK;

	return (int)id;
}
#else
unsigned int get_carrierboard_id(void)
{
	char const *str_id;
	u32 id;

	str_id = env_get("board_id");

	if (str_id != NULL) {
		strtou32(str_id, 10, &id);
		return id;
	}

	return CARRIERBOARD_ID_UNDEFINED;
}
#endif /* CONFIG_CARRIERBOARD_ID_ON_OTP */
#else
unsigned int get_carrierboard_id(void)
{
	return CARRIERBOARD_ID_UNDEFINED;
}
#endif /* CONFIG_HAS_CARRIERBOARD_ID */

__weak void fdt_fixup_carrierboard(void *fdt)
{
#if defined(CONFIG_HAS_CARRIERBOARD_VERSION) || \
    defined(CONFIG_HAS_CARRIERBOARD_ID)
	char str[20];
#endif

	/*
	 * Re-read board version/ID in case the shadow registers were
	 * overridden by the user.
	 */
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	board_version = get_carrierboard_version();
	sprintf(str, "%d", board_version);
	do_fixup_by_path(fdt, "/", "digi,carrierboard,version", str,
			 strlen(str) + 1, 1);
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
	board_id = get_carrierboard_id();
	sprintf(str, "%d", board_id);
	do_fixup_by_path(fdt, "/", "digi,carrierboard,id", str,
			 strlen(str) + 1, 1);
#endif
}

__weak void print_carrierboard_info(void)
{
	char board_str[100] = "";

#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	if (CARRIERBOARD_VERSION_UNDEFINED == board_version)
		sprintf(board_str, "version undefined");
	else
		sprintf(board_str, "version %d", board_version);
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
	if (CARRIERBOARD_ID_UNDEFINED == board_id)
		sprintf(board_str, "%s, ID undefined", board_str);
	else
		sprintf(board_str, "%s, ID %d", board_str, board_id);
#endif

	if (strcmp(board_str, ""))
		printf("  Board %s\n", board_str);
}
