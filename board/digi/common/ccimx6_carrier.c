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
#include <fuse.h>

unsigned int get_carrierboard_version(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_VERSION
	u32 version;

	if (fuse_read(CONFIG_CARRIERBOARD_VERSION_BANK,
		      CONFIG_CARRIERBOARD_VERSION_WORD, &version))
		return CARRIERBOARD_VERSION_UNDEFINED;

	version >>= CONFIG_CARRIERBOARD_VERSION_OFFSET;
	version &= CONFIG_CARRIERBOARD_VERSION_MASK;

	return((int)version);
#else
	return CARRIERBOARD_VERSION_UNDEFINED;
#endif /* CONFIG_HAS_CARRIERBOARD_VERSION */
}

unsigned int get_carrierboard_id(void)
{
#ifdef CONFIG_HAS_CARRIERBOARD_ID
	u32 id;

	if (fuse_read(CONFIG_CARRIERBOARD_ID_BANK,
		      CONFIG_CARRIERBOARD_ID_WORD, &id))
		return CARRIERBOARD_ID_UNDEFINED;

	id >>= CONFIG_CARRIERBOARD_ID_OFFSET;
	id &= CONFIG_CARRIERBOARD_ID_MASK;

	return((int)id);
#else
	return CARRIERBOARD_ID_UNDEFINED;
#endif /* CONFIG_HAS_CARRIERBOARD_ID */
}
