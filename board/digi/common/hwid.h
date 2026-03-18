/*
 * Copyright 2014-2026 Digi International Inc. All Rights Reserved.
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
#elif defined(CONFIG_CCMP1)
#include "../ccmp1/hwid_ccmp1.h"
#elif defined(CONFIG_CCMP2)
#include "../ccmp2/hwid_ccmp2.h"
#endif

enum digi_cert {
	DIGI_CERT_USA = 0,
	DIGI_CERT_INTERNATIONAL,
	DIGI_CERT_JAPAN,

	DIGI_MAX_CERT,
};

struct digi_hwid_fuse {
	u32 bank;
	u32 word;
	u32 len;	/* length in nibbles (4-bits) */
};

void board_hwid_fuse_prog_unlock(void);
void board_hwid_fuse_prog_lock(void);
void board_hwid_print(const struct digi_hwid *hwid);
void board_hwid_print_hex(const struct digi_hwid *hwid);
void board_hwid_print_manuf(const struct digi_hwid *hwid);
int board_hwid_parse(int argc, char *const argv[], struct digi_hwid *hwid);
int board_hwid_parse_manuf(int argc, char *const argv[], struct digi_hwid *hwid);
int board_hwid_fuse_read(struct digi_hwid *hwid);
int board_hwid_fuse_set_local_vars(void);
int board_hwid_fuse_prog(const struct digi_hwid *hwid);
void board_hwid_update(bool is_fuse);
int board_hwid_fuse_lock(void);
void fdt_fixup_hwid(void *fdt, const struct digi_hwid *hwid);
void fdt_fixup_fuse_hwid(void *fdt);
u32 hwid_get_ramsize(const struct digi_hwid *hwid);
void hwid_get_macs(const struct digi_hwid *hwid);
void hwid_get_mac_pool(const struct digi_hwid *hwid, uint8_t *mac);
void hwid_get_serial_number(const struct digi_hwid *hwid);

int hwid_env_read(struct digi_hwid *hwid);
int hwid_env_prog(const struct digi_hwid *hwid);
int hwid_env_clear(void);
int hwid_read(struct digi_hwid *hwid);

#endif	/* __HWID_H_ */
