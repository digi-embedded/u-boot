/*
 * Copyright 2014-2019 Digi International Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __HWID_H_
#define __HWID_H_

#if defined(CONFIG_CC6)
#include "../ccimx6/hwid_cc6.h"
#elif defined(CONFIG_CC8X)
#include "../ccimx8x/hwid_cc8x.h"
#elif defined(CONFIG_CC8M)
#include "../ccimx8m/hwid_cc8m.h"
#endif

enum digi_cert {
	DIGI_CERT_USA = 0,
	DIGI_CERT_INTERNATIONAL,
	DIGI_CERT_JAPAN,

	DIGI_MAX_CERT,
};

void board_print_hwid(struct digi_hwid *hwid);
void board_print_manufid(struct digi_hwid *hwid);
int board_parse_hwid(int argc, char *const argv[], struct digi_hwid *hwid);
int board_parse_manufid(int argc, char *const argv[], struct digi_hwid *hwid);
int board_read_hwid(struct digi_hwid *hwid);
int board_sense_hwid(struct digi_hwid *hwid);
int board_prog_hwid(const struct digi_hwid *hwid);
int board_override_hwid(const struct digi_hwid *hwid);
void board_update_hwid(bool is_fuse);
int board_lock_hwid(void);
void fdt_fixup_hwid(void *fdt, const struct digi_hwid *hwid);
u64 hwid_get_ramsize(const struct digi_hwid *hwid);
void print_hwid_hex(struct digi_hwid *hwid);
void hwid_get_macs(uint32_t pool, uint32_t base);
void hwid_get_serial_number(uint32_t year, uint32_t week, uint32_t genid, uint32_t serial);

#endif	/* __HWID_H_ */
