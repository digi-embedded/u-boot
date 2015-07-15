/*
 * Copyright (C) 2012 Boundary Devices Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _ASM_BOOT_MODE_H
#define _ASM_BOOT_MODE_H
#define MAKE_CFGVAL(cfg1, cfg2, cfg3, cfg4) \
	((cfg4) << 24) | ((cfg3) << 16) | ((cfg2) << 8) | (cfg1)

struct boot_mode {
	const char *name;
	unsigned cfg_val;
};

struct boot_device {
	const char *name;
	unsigned val;
	unsigned mask;
};

void add_board_boot_modes(const struct boot_mode *p);
void boot_mode_apply(unsigned cfg_val);
const char * boot_mode_string(void);
extern const struct boot_mode soc_boot_modes[];
#endif
