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
#include "helper.h"

int get_carrierboard_version(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	char const *str_version;
	u32 version;

	str_version = env_get("board_version");

	if (str_version != NULL) {
		strtou32(str_version, 10, &version);
		return version;
	}
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */
	return CARRIERBOARD_VERSION_UNDEFINED;
}

int get_carrierboard_id(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_ID
	char const *str_id;
	u32 id;

	str_id = env_get("board_id");

	if (str_id != NULL) {
		strtou32(str_id, 10, &id);
		return id;
	}
#endif /* CONFIG_HAS_CARRIERBOARD_ID */
	return CARRIERBOARD_ID_UNDEFINED;
}
