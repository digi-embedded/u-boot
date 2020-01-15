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
#include <fdt_support.h>
#include "carrier_board.h"

#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
extern unsigned int board_version;
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
extern unsigned int board_id;
#endif

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
	char board_str[100];
	char warnings[100] = "";

	sprintf(board_str, "Board: %s %s", CONFIG_SOM_DESCRIPTION,
		CONFIG_BOARD_DESCRIPTION);
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	if (CARRIERBOARD_VERSION_UNDEFINED == board_version)
		sprintf(warnings, "%s   WARNING: Undefined board version!\n",
			warnings);
	else
		sprintf(board_str, "%s, version %d", board_str, board_version);
#endif

#ifdef CONFIG_HAS_CARRIERBOARD_ID
	if (CARRIERBOARD_ID_UNDEFINED == board_id)
		sprintf(warnings, "%s   WARNING: Undefined board ID!\n",
			warnings);
	else
		sprintf(board_str, "%s, ID %d", board_str, board_id);
#endif
	printf("%s\n", board_str);
	if (strcmp(warnings, ""))
		printf("%s", warnings);
}
