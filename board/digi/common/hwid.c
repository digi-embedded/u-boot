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
 #include <linux/errno.h>
 #include "hwid.h"
 #include <fdt_support.h>

 static void array_to_hwid(u32 *hwid_array, struct digi_hwid *hwid)
 {
 	/*
 	 * hwid_array is composed by 2 words: HWID0 and HWID1, where:
 	 *
 	 *        |  31..27  | 26..20 |             19..0            |
 	 *        +----------+--------+------------------------------+
 	 * HWID0: | Location |  GenID |          Serial number       |
 	 *        +----------+--------+------------------------------+
 	 *
 	 *        | 31..26 | 25..20 | 19..16 |  15..8  | 7..4 | 3..0 |
 	 *        +--------+--------+--------+---------+------+------+
 	 * HWID1: |  Year  |  Week  |  WID   | Variant |  HV  | Cert |
 	 *        +--------+--------+--------+---------+------+------+
 	 */
 	hwid->year = (hwid_array[1] >> 26) & 0x3f;
 	hwid->week = (hwid_array[1] >> 20) & 0x3f;
 	hwid->wid = (hwid_array[1] >> 16) & 0xf;
 	hwid->variant = (hwid_array[1] >> 8) & 0xff;
 	hwid->hv = (hwid_array[1] >> 4) & 0xf;
 	hwid->cert = hwid_array[1] & 0xf;
 	hwid->location = (hwid_array[0] >> 27) & 0x1f;
 	hwid->genid = (hwid_array[0] >> 20) & 0x7f;
 	hwid->sn = hwid_array[0] & 0xfffff;
 }

 __weak int board_get_hwid(struct digi_hwid *hwid)
 {
 	u32 hwid_array[HWID_ARRAY_WORDS_NUMBER];

 	board_read_hwid(hwid_array);
 	array_to_hwid(hwid_array, hwid);

 	return 0;
 }

 __weak void board_fdt_fixup_hwid(void *fdt, struct digi_hwid *hwid)
 {
 	const char *propnames[] = {
 		"digi,hwid,location",
 		"digi,hwid,genid",
 		"digi,hwid,sn",
 		"digi,hwid,year",
 		"digi,hwid,week",
 		"digi,hwid,variant",
 		"digi,hwid,hv",
 		"digi,hwid,cert",
 		"digi,hwid,wid",
 	};
 	char str[20];
 	int i;

 	/* Register the HWID as main node properties in the FDT */
 	for (i = 0; i < ARRAY_SIZE(propnames); i++) {
 		/* Convert HWID fields to strings */
 		if (!strcmp("digi,hwid,location", propnames[i]))
 			sprintf(str, "%c", hwid->location + 'A');
 		else if (!strcmp("digi,hwid,genid", propnames[i]))
 			sprintf(str, "%02d", hwid->genid);
 		else if (!strcmp("digi,hwid,sn", propnames[i]))
 			sprintf(str, "%06d", hwid->sn);
 		else if (!strcmp("digi,hwid,year", propnames[i]))
 			sprintf(str, "20%02d", hwid->year);
 		else if (!strcmp("digi,hwid,week", propnames[i]))
 			sprintf(str, "%02d", hwid->week);
 		else if (!strcmp("digi,hwid,variant", propnames[i]))
 			sprintf(str, "0x%02x", hwid->variant);
 		else if (!strcmp("digi,hwid,hv", propnames[i]))
 			sprintf(str, "0x%x", hwid->hv);
 		else if (!strcmp("digi,hwid,cert", propnames[i]))
 			sprintf(str, "0x%x", hwid->cert);
 		else if (!strcmp("digi,hwid,wid", propnames[i]))
 			sprintf(str, "0x%x", hwid->wid);
 		else
 			continue;

 		do_fixup_by_path(fdt, "/", propnames[i], str,
 				 strlen(str) + 1, 1);
 	}
 }
