/*
 * Copyright 2016 Digi International Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef CARRIER_BOARD_H
#define CARRIER_BOARD_H

unsigned int get_carrierboard_version(void);
unsigned int get_carrierboard_id(void);
void fdt_fixup_carrierboard(void *fdt);
void print_carrierboard_info(void);

#endif	/* CARRIER_BOARD_H */
