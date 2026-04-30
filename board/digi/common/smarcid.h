// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026, Digi International, Inc.
 */

#ifndef __SMARCID_H_
#define __SMARCID_H_

#if defined(CONFIG_IMX95)
#include "../ccimx95/smarcid_ccimx95.h"
#endif

/* Fuse operations */
int board_smarcid_fuse_read(struct digi_smarcid *smarcid);
int board_smarcid_fuse_set_local_vars(void);
int board_smarcid_fuse_prog(const struct digi_smarcid *smarcid);
int board_smarcid_fuse_lock(void);

/* Environment operations */
int smarcid_env_read(struct digi_smarcid *smarcid);
int smarcid_env_prog(const struct digi_smarcid *smarcid);
int smarcid_env_clear(void);

bool is_smarc(const struct digi_smarcid *smarcid);
int smarcid_read(struct digi_smarcid *smarcid);
void fdt_fixup_smarcid(void *fdt, const struct digi_smarcid *smarcid);
void fdt_fixup_fuse_smarcid(void *fdt);

void board_smarcid_print(const struct digi_smarcid *smarcid);
void board_smarcid_print_hex(const struct digi_smarcid *smarcid);
void board_smarcid_print_manuf(const struct digi_smarcid *smarcid);
int board_smarcid_parse(int argc, char *const argv[], struct digi_smarcid *smarcid);
int board_smarcid_parse_manuf(int argc, char *const argv[], struct digi_smarcid *smarcid);
void board_smarcid_update(void);

char board_smarcid_get_location_char(int location_index);
void smarcid_get_serial_number(const struct digi_smarcid *smarcid);
void smarcid_get_variant(const struct digi_smarcid *smarcid);

#endif	/* __SMARCID_H_ */
